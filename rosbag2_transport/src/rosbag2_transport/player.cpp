// Copyright 2018, Bosch Software Innovations GmbH.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "player.hpp"

#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <utility>

#include "rclcpp/rclcpp.hpp"
#include "rcl/graph.h"
#include "rcutils/time.h"

#include "rosbag2/sequential_reader.hpp"
#include "rosbag2/typesupport_helpers.hpp"
#include "rosbag2_transport/logging.hpp"
#include "rosbag2_node.hpp"
#include "replayable_message.hpp"

namespace rosbag2_transport
{

const std::chrono::milliseconds
Player::queue_read_wait_period_ = std::chrono::milliseconds(100);

Player::Player(
  std::shared_ptr<rosbag2::SequentialReader> reader, std::shared_ptr<Rosbag2Node> rosbag2_transport)
: reader_(std::move(reader)), rosbag2_transport_(rosbag2_transport)
{}

bool Player::is_storage_completely_loaded() const
{
  if (storage_loading_future_.valid() &&
    storage_loading_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
  {
    storage_loading_future_.get();
  }
  return !storage_loading_future_.valid();
}

void Player::play(const PlayOptions & options)
{
  prepare_publishers();

  storage_loading_future_ = std::async(std::launch::async,
      [this, options]() {load_storage_content(options);});

  wait_for_filled_queue(options);

  key_control_future_ = std::async(std::launch::async,
      [this]() {get_key_control();});

  playing_status_future_ = std::async(std::launch::async,
      [this]() {print_playing_status();});

  play_messages_from_queue();
}

void Player::switch_pause_status()
{
  if(playing_status_ == PAUSE)
  {
    playing_status_ = PLAYING;
    playing_status_string_ = "RUNNING";
    // std::cout << "Play status switch to [ PLAYING ] " << std::endl;
  }
  else
  {
    // std::cout << "Play status switch to [ PAUSE ] " << std::endl;
    playing_status_ = PAUSE;
    playing_status_string_ = "PAUSED";
  }
}

#include <termio.h>
#include <sys/types.h>
#include <unistd.h>

static struct termios ori_attr, cur_attr;
static __inline
    int tty_reset(void)
    {
            if (tcsetattr(STDIN_FILENO, TCSANOW, &ori_attr) != 0)
                    return -1;
 
            return 0;
    }

static __inline
    int tty_set(void)
    {
           
            if ( tcgetattr(STDIN_FILENO, &ori_attr) )
                    return -1;
           
            memcpy(&cur_attr, &ori_attr, sizeof(cur_attr) );
            cur_attr.c_lflag &= ~ICANON;
    //        cur_attr.c_lflag |= ECHO;
            cur_attr.c_lflag &= ~ECHO;
            cur_attr.c_cc[VMIN] = 1;
            cur_attr.c_cc[VTIME] = 0;
 
            if (tcsetattr(STDIN_FILENO, TCSANOW, &cur_attr) != 0)
                    return -1;
 
            return 0;

    }

static __inline
int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

static __inline
int getch(void)
{
     struct termios tm, tm_old;
     int fd = 0, ch;

     if (tcgetattr(fd, &tm) < 0) {
          return -1;
     }

     tm_old = tm;
     cfmakeraw(&tm);
     if (tcsetattr(fd, TCSANOW, &tm) < 0) {
          return -1;
     }

     ch = getchar();
     if (tcsetattr(fd, TCSANOW, &tm_old) < 0) {
          return -1;
     }

     return ch;
}

static __inline
int tc_flush_in()
{
  int fd = 0;
  if(tcflush(fd, TCIFLUSH) < 0)
  {
    return -1;
  }
  return 0;
}

void Player::print_playing_status() const
{
  printf("\n");
  while (rclcpp::ok() && !finished_)
  {
    printf("\r[%-7s] Bag Time: %.3f; Duration: %.3f",
		playing_status_string_.c_str(), 
		std::chrono::nanoseconds((playing_time_)).count() * 1e-9,
		std::chrono::nanoseconds((playing_time_ - bag_start_time_)).count() * 1e-9
		// std::chrono::nanoseconds((total_time_)).count() * 1e-9
		); 
    fflush(stdout); 
    std::this_thread::sleep_for(queue_read_wait_period_);
  }
  // printf("tread exit\n");
}

void Player::get_key_control()
{
  // std::cout << "get key control thread " << std::endl;
  int tty_set_flag;
  tty_set_flag = tty_set();

  while (rclcpp::ok() && !finished_)
  {
    if(kbhit())
    {
      char ch = getch();
      // std::cout << "get input " << ch << std::endl;
      if(ch == ' ')
      {
        switch_pause_status();
      }
      else if(ch == 0x03) break;    
      else if(ch == 's')
      {
        play_message_one_step();
      }
      if(!finished_) tc_flush_in();
    }
    std::this_thread::sleep_for(queue_read_wait_period_);
  }
  if(tty_set_flag == 0) tty_reset();
  // printf("key over\n");
}

void Player::wait_for_filled_queue(const PlayOptions & options) const
{
  while (
    message_queue_.size_approx() < options.read_ahead_queue_size &&
    !is_storage_completely_loaded() && rclcpp::ok())
  {
    std::this_thread::sleep_for(queue_read_wait_period_);
  }
}

void Player::load_storage_content(const PlayOptions & options)
{
  TimePoint time_first_message;

  ReplayableMessage message;  
  if (reader_->has_next()) {
    message.message = reader_->read_next();
    message.time_since_start = std::chrono::nanoseconds(0);
    bag_start_time_ = message.message->time_stamp + options.start_time * 1e9;
    while(reader_->has_next() && rclcpp::ok())
    {
      if(message.message->time_stamp >= bag_start_time_)
      {
        break; 
      }
      message.message = reader_->read_next();
    }
    bag_start_time_ = message.message->time_stamp;
    time_first_message = TimePoint(std::chrono::nanoseconds(message.message->time_stamp));
    message_queue_.enqueue(message);
  }

  auto queue_lower_boundary =
    static_cast<size_t>(options.read_ahead_queue_size * read_ahead_lower_bound_percentage_);
  auto queue_upper_boundary = options.read_ahead_queue_size;

  while (reader_->has_next() && rclcpp::ok()) {
    if (message_queue_.size_approx() < queue_lower_boundary) {
      enqueue_up_to_boundary(time_first_message, queue_upper_boundary);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void Player::enqueue_up_to_boundary(const TimePoint & time_first_message, uint64_t boundary)
{
  ReplayableMessage message;
  for (size_t i = message_queue_.size_approx(); i < boundary; i++) {
    if (!reader_->has_next()) {
      break;
    }
    message.message = reader_->read_next();
    message.time_since_start =
      TimePoint(std::chrono::nanoseconds(message.message->time_stamp)) - time_first_message;

    message_queue_.enqueue(message);
    // total_time_ = message.message->time_stamp - bag_start_time_;
  }
}

void Player::play_messages_from_queue()
{
  start_time_ = std::chrono::system_clock::now();
  do {
    play_messages_until_queue_empty();
    if (!is_storage_completely_loaded() && rclcpp::ok()) {
      ROSBAG2_TRANSPORT_LOG_WARN("Message queue starved. Messages will be delayed. Consider "
        "increasing the --read-ahead-queue-size option.");
    }
  } while (!is_storage_completely_loaded() && rclcpp::ok());
}

void Player::play_message_one_step()
{
  if(playing_status_ != PAUSE) return;
  ReplayableMessage message;
  if (message_queue_.try_dequeue(message) && rclcpp::ok()) {    
    if (rclcpp::ok()) {
      publishers_[message.message->topic_name]->publish(message.message->serialized_data);
    }
    playing_time_ = message.message->time_stamp;      
  }
}

void Player::play_messages_until_queue_empty()
{
  finished_ = false;
  ReplayableMessage message;
  while (message_queue_.try_dequeue(message) && rclcpp::ok()) {
    static PlayingStatus last_status = playing_status_;
    if(playing_status_ == PLAYING)
    {
      if(last_status == PAUSE)
      {
        start_time_ = std::chrono::system_clock::now() - message.time_since_start;
      }
      else std::this_thread::sleep_until(start_time_ + message.time_since_start);
      if (rclcpp::ok()) {
        publishers_[message.message->topic_name]->publish(message.message->serialized_data);
      }
      playing_time_ = message.message->time_stamp;
      last_status = PLAYING;
    }
    else
    {
      // start_time_ += queue_read_wait_period_;
      while(playing_status_ == PAUSE && rclcpp::ok())
      {
        std::this_thread::sleep_for(queue_read_wait_period_);      
      }
      last_status = PAUSE;
    }    
  }
  finished_ = true;
  std::this_thread::sleep_for(queue_read_wait_period_ * 2);
  printf("\r\n[Finish ]\r\n");
}

void Player::prepare_publishers()
{
  auto topics = reader_->get_all_topics_and_types();
  for (const auto & topic : topics) {
    publishers_.insert(std::make_pair(
        topic.name, rosbag2_transport_->create_generic_publisher(topic.name, topic.type)));
  }
}

}  // namespace rosbag2_transport
