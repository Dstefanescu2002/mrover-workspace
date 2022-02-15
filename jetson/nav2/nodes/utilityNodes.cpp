#include "utilityNodes.hpp"
#include "utilities.hpp"
#include "rover.hpp"

namespace utilNodes{

    BT::NodeStatus emptyCourse() {
        if (gRover->roverStatus().course().empty()) {
            cout << "Final Node: emptyCourse\n";
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus isOff(){
        if ( !gRover->roverStatus().autonState().is_auton ){
            cout << "Final Node: isOff\n";
            return BT::NodeStatus::SUCCESS;
        }
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus atCurrentWaypoint(){
        cout << gRover->roverStatus().course().empty() << '\n';
        if ( gRover->roverStatus().course().empty() || estimateNoneuclid(gRover->roverStatus().odometry(),
                               gRover->roverStatus().course().front().odom) < 1.0 ) { // TODO determine distance threshold
            return BT::NodeStatus::SUCCESS;
        }
        cout << "Final Node: AtCurrentWaypoint\n";
        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus spinGimbal(){
        // spins gimbal
        // Fails: if at the end of the gimbal angles trajectory
        // Succeeds: otherwise
        gRover->sendGimbalSetpoint( gRover->gimbalAngles()[gRover->gimbalIndex()] );

        if ( gRover->gimbalIndex() == (int)gRover->gimbalAngles().size()-1){
            // at end of spin cycle - haven't seen target, give up
            gRover->gimbalIndex() = 0;
            return BT::NodeStatus::FAILURE;
        }
        else{
            gRover->gimbalIndex()++;
            return BT::NodeStatus::SUCCESS;
        }
    }

    BT::NodeStatus resetGimbalIndex(){
        // resets the gimbal index to 0 such that the trajectory of the
        // gimbal spin is reset
        // TODO may need to point the gimbal forward again after the spin manuever is done
        gRover->gimbalIndex() = 0;
        return BT::NodeStatus::SUCCESS;
    }

    BT::NodeStatus popCourse(){
        gRover->roverStatus().course().pop_front();
        return BT::NodeStatus::SUCCESS;
    }

    BT::NodeStatus isDestinationPoint(){
        if ( gRover->roverStatus().course().front().type == "destination" ){
            return BT::NodeStatus::SUCCESS;
        }

        return BT::NodeStatus::FAILURE;

    }

    BT::NodeStatus isARTagLeg(){
        // gate = false
        // search = true
        if ( gRover->roverStatus().course().front().gate == false &&
             gRover->roverStatus().course().front().search == true ){

            return BT::NodeStatus::SUCCESS;
        }

        return BT::NodeStatus::FAILURE;
    }

    BT::NodeStatus clearCourse(){
        gRover->roverStatus().course().clear();
        return BT::NodeStatus::SUCCESS;

    }

    BT::NodeStatus turnOff(){
        gRover->roverStatus().autonState().is_auton = false;
        return BT::NodeStatus::SUCCESS;
    }

    
    
    void registerNodes(BT::BehaviorTreeFactory& factory){
        factory.registerSimpleAction( "IsOff", std::bind(isOff) );
        factory.registerSimpleAction( "EmptyCourse", std::bind(emptyCourse) );
        factory.registerSimpleAction( "AtCurrentWaypoint", std::bind(atCurrentWaypoint) );
        factory.registerSimpleAction( "SpinGimbal", std::bind(spinGimbal) );
        factory.registerSimpleAction( "ResetGimbalIndex", std::bind(resetGimbalIndex) );
        factory.registerSimpleAction( "PopCourse", std::bind(popCourse) );
        factory.registerSimpleAction( "IsDestinationPoint", std::bind(isDestinationPoint) );
        factory.registerSimpleAction( "IsARTagLeg", std::bind(isARTagLeg) );
        factory.registerSimpleAction( "ClearCourse", std::bind(clearCourse) );
        factory.registerSimpleAction( "TurnOff", std::bind(turnOff) );
    }
}


