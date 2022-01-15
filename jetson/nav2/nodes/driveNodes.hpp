#include "behaviortree_cpp_v3/behavior_tree.h"
#include "behaviortree_cpp_v3/bt_factory.h"
namespace driveNodes {
    void registerNodes(BT::BehaviorTreeFactory& factory);

    
    BT::NodeStatus driveToWaypoint(); // unused?

    BT::NodeStatus driveToPoint();



}