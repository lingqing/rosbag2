^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package rosbag2_tests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Forthcoming
-----------
* [backport] ros1 dependency handling (`#98 <https://github.com/ros2/rosbag2/issues/98>`_)
  * removed dependency to ros1_bridge package (`#90 <https://github.com/ros2/rosbag2/issues/90>`_)
  * removed dependency to ros1_bridge package:
  * checking if package is available
  * if not skipping (with warnings)
  * now rosbag2_tests builds on systems without ros1
  * check ros1 deps correctly on all packages
  * add ros1_bridge to test package
  * silently try to find the bridge
  * correct missing linter errors (`#96 <https://github.com/ros2/rosbag2/issues/96>`_)
  Signed-off-by: Karsten Knese <karsten@openrobotics.org>
* Contributors: Karsten Knese

0.0.6 (2019-02-27)
------------------

0.0.5 (2018-12-27)
------------------

0.0.4 (2018-12-19)
------------------
* 0.0.3
* Play old bagfiles (`#69 <https://github.com/bsinno/rosbag2/issues/69>`_)
* Contributors: Karsten Knese, Martin Idel

0.0.2 (2018-12-12)
------------------
* do not ignore return values
* update maintainer email
* Contributors: Karsten Knese, root

0.0.1 (2018-12-11)
------------------
* Auto discovery of new topics (`#63 <https://github.com/ros2/rosbag2/issues/63>`_)
* Split converters (`#70 <https://github.com/ros2/rosbag2/issues/70>`_)
* Fix master build and small renamings (`#67 <https://github.com/ros2/rosbag2/issues/67>`_)
* rename topic_with_types to topic_metadata
* iterate_over_formatter
* GH-142 replace map with unordered map where possible (`#65 <https://github.com/ros2/rosbag2/issues/65>`_)
* Use converters when recording a bag file (`#57 <https://github.com/ros2/rosbag2/issues/57>`_)
* Display bag summary using `ros2 bag info` (`#45 <https://github.com/ros2/rosbag2/issues/45>`_)
* Use directory as bagfile and add additonal record options (`#43 <https://github.com/ros2/rosbag2/issues/43>`_)
* Introduce rosbag2_transport layer and CLI (`#38 <https://github.com/ros2/rosbag2/issues/38>`_)
* Contributors: Alessandro Bottero, Andreas Greimel, Andreas Holzner, Karsten Knese, Martin Idel
