#include "gateNodes.hpp"
#include "rover.hpp"
#include "utilities.hpp"
#include "rover_msgs/Waypoint.hpp"
#include "rover_msgs/Odometry.hpp"

#include <math.h>


namespace gateNodes{

    BT::NodeStatus isFirstGatePostLocKnown(){
        if ( gRover->roverStatus().firstGatePostFound() ){
            return BT::NodeStatus::SUCCESS;
        } else {
            return BT::NodeStatus::FAILURE;
        }
    }

    BT::NodeStatus genGateTraversalPath(){

        if (genGateTraversalPathHelper()){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    bool genGateTraversalPathHelper(){
        // leftGate ID = 4
        // rightGate ID = 5
        // 2 meters wide
        //camera depth 4-4.5 meters, BEST is 5m
        // waypoints should be no less than 1.5 meters from gate center

        /*
            alternative to calculate new odom for w1/w2
            (using linear equations/slopes)

            lat_mid = (lat1 + lat2) / 2
            long_mid = (long1 + long2) / 2
            m = (lat1 - lat2) / (long1 - long2)
            lat_waypoint1  = lat_mid  + sqrt(DIST^2 / (1 + 1/(m*m)))
            long_waypoint1 = long_mid + sqrt(DIST^2 / (1 + (m*m)))
        */
        // TODO move these to json constants file
        const double BETWEEN_POSTS = 2;  // space between posts in meters
        const double DIST_MID_GATE = 1.6;  // distance from the midpoint of the gate to first traversal point
        const double DIST = sqrt(pow(BETWEEN_POSTS/2, 2) + pow(DIST_MID_GATE,2));  // distance from left post to first traversal point
        const uint8_t LEFT_GATE_ID = 4;
        const uint8_t RIGHT_GATE_ID = 5;


        Odometry left_post;
        Odometry right_post;
        // note: path will be generated if at least one post (ID) is identified correctly
        if ( gRover->roverStatus().post1().id == LEFT_GATE_ID ) {
            left_post = gRover->roverStatus().post1().location;
            right_post = gRover->roverStatus().post2().location;
        }
        else if ( gRover->roverStatus().post1().id == RIGHT_GATE_ID ){
            right_post = gRover->roverStatus().post1().location;
            left_post = gRover->roverStatus().post2().location;
        }
        else if ( gRover->roverStatus().post2().id == LEFT_GATE_ID ) {
            right_post = gRover->roverStatus().post1().location;
            left_post = gRover->roverStatus().post2().location;
        }
        else if ( gRover->roverStatus().post2().id == RIGHT_GATE_ID ){
            left_post = gRover->roverStatus().post1().location;
            right_post = gRover->roverStatus().post2().location;
        }
        else {
            // TODO figure out failure/recover functionality
            return false;
        }

        double phi = calcBearing( left_post, right_post );

        double theta1 = radianToDegree(atan2(DIST,BETWEEN_POSTS/2));

        // TODO implement this correctly
        Waypoint w1;
        w1.type = "gateTraversal";
        w1.odom = createOdom( left_post, phi+theta1, DIST );

        Waypoint w2;
        w2.type = "gateTraversal";
        w2.odom = createOdom( left_post, phi-theta1, DIST );

        double bearing = calcBearing( w1.odom,w2.odom );

        w1.odom.bearing_deg = bearing;
        w2.odom.bearing_deg = bearing;

        gRover->roverStatus().course().push_back( w1 );
        gRover->roverStatus().course().push_back( w2 );

        return true;
    }

    BT::NodeStatus isGateTraversalPoint(){
        if ( gRover->roverStatus().course().front().type == "gateTraversal" ){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus hasGateTraversalPoints(){
        // TODO do we have to look at the top point or the next point?
        // this node is the same as isGateTraversalPoint() that may change
        if ( gRover->roverStatus().course().front().type == "gateTraversal" ){
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus verifyGateTraversal(){
        /*
        if succeed: go to regen gate traversal
        if fail: go to turn off
        TODO decide functionality when the gate IDs are not what's expected (regen or not?)
        */

        // note: the left and right posts will be flipped relative to the rover
        // if the rover is in the correct orientation
        const uint8_t LEFT_GATE_ID = 4;
        const uint8_t RIGHT_GATE_ID = 5;

        // if we see two targets check if we traversed in the correct direction
        if ( gRover->roverStatus().target().distance >= 0 && gRover->roverStatus().target2().distance >= 0 ){

            if ( gRover->roverStatus().target().id == LEFT_GATE_ID && gRover->roverStatus().target2().id == RIGHT_GATE_ID ){

                double left_angle = (gRover->roverStatus().target().bearing > 180.0) ? gRover->roverStatus().target().bearing - 360.0: gRover->roverStatus().target().bearing;
                double right_angle = (gRover->roverStatus().target2().bearing > 180.0) ? gRover->roverStatus().target2().bearing - 360.0: gRover->roverStatus().target2().bearing;;

                //condition flipped since we are on the opposite side of the gate
                if (right_angle > left_angle){
                    // flip posts (went through wrong way)
                    uint8_t temp = gRover->roverStatus().post1().id;
                    gRover->roverStatus().post1().id = gRover->roverStatus().post2().id;
                    gRover->roverStatus().post2().id = temp;
                    return BT::NodeStatus::SUCCESS;
                }

            }
            else if ( gRover->roverStatus().target().id == RIGHT_GATE_ID && gRover->roverStatus().target2().id == LEFT_GATE_ID ){

                double left_angle = (gRover->roverStatus().target2().bearing > 180.0)
                                    ? gRover->roverStatus().target2().bearing - 360.0: gRover->roverStatus().target2().bearing;
                double right_angle = (gRover->roverStatus().target().bearing > 180.0)
                                    ? gRover->roverStatus().target().bearing - 360.0: gRover->roverStatus().target().bearing;

                // condition flipped since we are on the opposite side of the gate
                if (right_angle > left_angle){
                    // regenTraversal path
                    // flip posts (went through wrong way)
                    uint8_t temp = gRover->roverStatus().post1().id;
                    gRover->roverStatus().post1().id = gRover->roverStatus().post2().id;
                    gRover->roverStatus().post2().id = temp;
                    return BT::NodeStatus::SUCCESS;
                }
            }
                // we see the targets but they are not gate IDs
                return BT::NodeStatus::FAILURE;
        }
        return BT::NodeStatus::FAILURE; //double check logic
        // warning: control reaches end of non-void function [-Wreturn-type]
    }

    BT::NodeStatus genSecondPostSearchPattern(){
        generateDiamondGateSearch();
        return BT::NodeStatus::SUCCESS;
    }


    void registerNodes( BT::BehaviorTreeFactory& factory ){

        factory.registerSimpleAction( "IsFirstGatePostLocKnown", std::bind(isFirstGatePostLocKnown) );
        factory.registerSimpleAction( "GenGateTraversalPath", std::bind(genGateTraversalPath) );
        factory.registerSimpleAction( "IsGateTraversalPoint", std::bind(isGateTraversalPoint) );
        factory.registerSimpleAction( "HasGateTraversalPoints", std::bind(hasGateTraversalPoints) );
        factory.registerSimpleAction( "VerifyGateTraversal", std::bind(verifyGateTraversal) );
        factory.registerSimpleAction( "GenSecondPostSearchPattern", std::bind(genSecondPostSearchPattern) );

    }

}