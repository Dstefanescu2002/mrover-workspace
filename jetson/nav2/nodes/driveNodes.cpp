#include "driveNodes.hpp"
#include "rover.hpp"

namespace driveNodes{

    //TODO
    BT::NodeStatus driveToWaypoint(){
        return BT::NodeStatus::SUCCESS;
    }

    //TODO
    BT::NodeStatus driveToPoint(){
        return BT::NodeStatus::SUCCESS;

    }

    void registerNodes(BT::BehaviorTreeFactory& factory){
        factory.registerSimpleAction( "DriveToWaypoint", std::bind(driveToWaypoint) );
        factory.registerSimpleAction( "DriveToPoint", std::bind(driveToPoint) );
    }

}