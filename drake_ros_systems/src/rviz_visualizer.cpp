// Copyright 2021 Open Source Robotics Foundation, Inc.
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

#include <drake/systems/framework/diagram_builder.h>

#include <rclcpp/qos.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

#include <memory>
#include <unordered_set>
#include <utility>

#include "drake_ros_systems/constant_message_source.hpp"
#include "drake_ros_systems/ros_publisher_system.hpp"
#include "drake_ros_systems/scene_markers_system.hpp"
#include "drake_ros_systems/scene_tf_broadcaster_system.hpp"

#include "drake_ros_systems/rviz_visualizer.hpp"


namespace drake_ros_systems
{

class RvizVisualizer::RvizVisualizerPrivate
{
public:
  SceneMarkersSystem * scene_visual_markers;
  SceneMarkersSystem * scene_collision_markers;
  SceneTfBroadcasterSystem * scene_tf_broadcaster{nullptr};
};

RvizVisualizer::RvizVisualizer(
  std::shared_ptr<DrakeRosInterface> ros_interface,
  RvizVisualizerParams params)
: impl_(new RvizVisualizerPrivate())
{
  drake::systems::DiagramBuilder<double> builder;

  auto scene_visual_markers_publisher = builder.AddSystem(
    RosPublisherSystem::Make<visualization_msgs::msg::MarkerArray>(
      "/scene_markers/visual", rclcpp::QoS(1), ros_interface,
      params.publish_triggers, params.publish_period));

  impl_->scene_visual_markers =
    builder.AddSystem<SceneMarkersSystem>(
    SceneMarkersParams::Illustration());

  builder.Connect(
    impl_->scene_visual_markers->get_markers_output_port(),
    scene_visual_markers_publisher->get_input_port());

  builder.ExportInput(
    impl_->scene_visual_markers->get_graph_query_port(), "graph_query");

  auto scene_collision_markers_publisher = builder.AddSystem(
    RosPublisherSystem::Make<visualization_msgs::msg::MarkerArray>(
      "/scene_markers/collision", rclcpp::QoS(1), ros_interface,
      params.publish_triggers, params.publish_period));

  impl_->scene_collision_markers =
    builder.AddSystem<SceneMarkersSystem>(
    SceneMarkersParams::Proximity());

  builder.Connect(
    impl_->scene_collision_markers->get_markers_output_port(),
    scene_collision_markers_publisher->get_input_port());

  builder.ConnectInput(
    "graph_query", impl_->scene_collision_markers->get_graph_query_port());

  if (params.publish_tf) {
    impl_->scene_tf_broadcaster =
      builder.AddSystem<SceneTfBroadcasterSystem>(
      ros_interface, SceneTfBroadcasterParams{
        params.publish_triggers, params.publish_period});

    builder.ConnectInput(
      "graph_query", impl_->scene_tf_broadcaster->get_graph_query_port());
  }

  builder.BuildInto(this);
}

RvizVisualizer::~RvizVisualizer()
{
}

void
RvizVisualizer::RegisterMultibodyPlant(
  const drake::multibody::MultibodyPlant<double> * plant)
{
  impl_->scene_visual_markers->RegisterMultibodyPlant(plant);
  impl_->scene_collision_markers->RegisterMultibodyPlant(plant);
  if (impl_->scene_tf_broadcaster) {
    impl_->scene_tf_broadcaster->RegisterMultibodyPlant(plant);
  }
}

const drake::systems::InputPort<double> &
RvizVisualizer::get_graph_query_port() const
{
  return get_input_port();
}

}  // namespace drake_ros_systems