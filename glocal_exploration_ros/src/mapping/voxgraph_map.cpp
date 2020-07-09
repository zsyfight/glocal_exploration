#include "glocal_exploration_ros/mapping/voxgraph_map.h"

namespace glocal_exploration {

VoxgraphMap::VoxgraphMap(const std::shared_ptr<StateMachine>& state_machine)
    : MapBase(state_machine) {}

bool VoxgraphMap::setupFromConfig(MapBase::Config* config) {
  CHECK_NOTNULL(config);
  auto cfg = dynamic_cast<Config*>(config);
  if (!cfg) {
    LOG(ERROR)
        << "Failed to setup: config is not of type 'VoxgraphMap::Config'.";
    return false;
  }
  config_ = *cfg;

  // Launch the sliding window local map and global map servers
  ros::NodeHandle nh(ros::names::parentNamespace(config_.nh_private_namespace));
  ros::NodeHandle nh_voxblox(config_.nh_private_namespace, "local");
  voxblox_server_ = std::make_unique<voxblox::EsdfServer>(nh, nh_voxblox);
  ros::NodeHandle nh_voxgraph(config_.nh_private_namespace, "global");
  voxgraph_server_ =
      std::make_unique<voxgraph::VoxgraphMapper>(nh, nh_voxgraph);

  // Cached params
  c_voxel_size_ = voxblox_server_->getEsdfMapPtr()->voxel_size();
  c_block_size_ = voxblox_server_->getEsdfMapPtr()->block_size();
}

double VoxgraphMap::getVoxelSize() { return c_voxel_size_; }

bool VoxgraphMap::isTraversableInActiveSubmap(
    const Eigen::Vector3d& position, const Eigen::Quaterniond& orientation) {
  if (!state_machine_->pointInROI(position)) {
    return false;
  }
  double distance = 0.0;
  if (voxblox_server_->getEsdfMapPtr()->getDistanceAtPosition(position,
                                                              &distance)) {
    // This means the voxel is observed
    return (distance > config_.traversability_radius);
  }
  return (position - state_machine_->currentPose().position()).norm() <
      config_.clearing_radius;
}

// TODO(victorr): Replace dummy code and actually check in overlapping submaps
MapBase::VoxelState VoxgraphMap::getVoxelStateInLocalArea(
    const Eigen::Vector3d& point) {
  double distance = 0.0;
  if (voxblox_server_->getEsdfMapPtr()->getDistanceAtPosition(point,
                                                              &distance)) {
    // This means the voxel is observed
    if (distance > c_voxel_size_) {
      return VoxelState::Free;
    }
    return VoxelState::Occupied;
  }
  return VoxelState::Unknown;
}

Eigen::Vector3d VoxgraphMap::getVoxelCenterInLocalArea(const Eigen::Vector3d& point) {
  return (point / c_voxel_size_).array().round() * c_voxel_size_;
}

}  // namespace glocal_exploration