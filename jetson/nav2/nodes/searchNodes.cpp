#include "searchNodes.hpp"
#include "utilities.hpp"
#include "rover_msgs/Odometry.hpp"

namespace searchNodes{
    BT::NodeStatus isSearchPoint(){
        if (gRover->roverStatus().course().front().type == "search"){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;

    }

    BT::NodeStatus isTargetApproachPoint(){
        if (gRover->roverStatus().course().front().type == "targetApproach"){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus hasTarget1(){
        if (gRover->roverStatus().target().distance > 0.0){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus hasTarget2(){
        if (gRover->roverStatus().target2().distance > 0.0){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus populateFirstTarget() {
        Odometry gateLoc = createOdom(gRover->roverStatus().odometry(), gRover->roverStatus().target().bearing, gRover->roverStatus().target().distance, gRover);
        gRover->roverStatus().post1().id = gRover->roverStatus().target().id;
        gRover->roverStatus().post1().location = gateLoc;
        return BT::NodeStatus::SUCCESS;
    }

    BT::NodeStatus populateSecondTarget() {
        Odometry gateLoc = createOdom(gRover->roverStatus().odometry(), gRover->roverStatus().target2().bearing, gRover->roverStatus().target2().distance, gRover);
        gRover->roverStatus().post2().id = gRover->roverStatus().target2().id;
        gRover->roverStatus().post2().location = gateLoc;
        return BT::NodeStatus::SUCCESS;
    }


    BT::NodeStatus putTargetAtFrontOfCourse(){
        Odometry locationToGo = gRover->roverStatus().post1().location;
        Waypoint waypoint;
        waypoint.odom = locationToGo;
        waypoint.type = "targetApproach";
        gRover->roverStatus().course().push_front(waypoint);
        return BT::NodeStatus::SUCCESS;
    }

    BT::NodeStatus generateSearchPoints(){
        /*
        at the end of this function the Course will be filled with waypoints centered at the rovers current location describing a search pattern
        The first pattern is the spiral out, then the lawnmower, then spiral in
        */
        generateSpiralOutPattern();
        generateLawnmowerSearchPattern();
        generateSpiralInPattern();
        return BT::NodeStatus::SUCCESS;
    }


    void registerNodes(BT::BehaviorTreeFactory& factory){
        factory.registerSimpleCondition("IsSearchPoint", std::bind(isSearchPoint));
        factory.registerSimpleCondition("IsTargetApproachPoint", std::bind(isTargetApproachPoint));
        factory.registerSimpleCondition("HasTarget1", std::bind(hasTarget1));
        factory.registerSimpleCondition("HasTarget2", std::bind(hasTarget2));
        factory.registerSimpleCondition("PopulateFirstTarget", std::bind(populateFirstTarget));
        factory.registerSimpleCondition("PopulateSecondTarget", std::bind(populateSecondTarget));
        factory.registerSimpleCondition("PutTargetAtFrontOfCourse", std::bind(putTargetAtFrontOfCourse));
        factory.registerSimpleCondition("GenSearchPoints", std::bind(generateSearchPoints));
    }
}