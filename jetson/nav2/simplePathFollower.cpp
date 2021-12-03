#include "simplePathFollower.hpp"
#include "rover.hpp"
SimplePathFollower::SimplePathFollower( lcm::LCM& lcmObject )
    : mRover( nullptr )
    , mLcmObject( lcmObject )
    , mTotalWaypoints( 0 )
    , mCompletedWaypoints( 0 )
    , mRepeaterDropComplete ( false )
    , mStateChanged( true )
{
    ifstream configFile;
    string configPath = getenv("MROVER_CONFIG");
    configPath += "/config_nav/config.json";
    configFile.open( configPath );
    string config = "";
    string setting;
    while( configFile >> setting )
    {
        config += setting;
    }
    configFile.close();
    mRoverConfig.Parse( config.c_str() );
}

void SimplePathFollower::followPath(std::vector<Odometry>& path){
    // //check if angle to target is greater than x degrees, if so turn to target
    // //else move forwards toward target with corrective factor for bearing

    // const int TURN_THRESH = 10;

    // //TODO: fill these variables in with actual rover state values
    // Odometry curOdom;

    // //TODO: calculate bearing to target (def exists somewhere in repo)
    // double bearingToTarget = 0.0;

    // //TODO: calculate dist to target (def alr exists somewhere in repo)
    // double distToTarget = 0.0;

    // double leftPower = 0.0;
    // double rightPower = 0.0;

    // if (abs(bearingToTarget) > TURN_THRESH){
    //     //turn to target
    //     //TODO: implement PID for this as well (alr exists somewhere)
    //     double turningPower = bearingToTarget;
    // }
    // else{
    //     //move forward to target
    //     //TODO: implement PID or some sort of function to control forward power based on distance (also def alr exists)
    //     double forwardPower = distToTarget;
    //     //TODO: implement PID or some sort of function to control turning correction power (def a util function or smthing alr written)
    //     double turningPower = bearingToTarget;

    //     leftPower = forwardPower + turningPower;
    //     rightPower = forwardPower - turningPower;
    // }

    // //TODO: set left and right powers to the rover (call a function)
    current_state = DriveState::Drive;
    while (current_state != DriveState::Done) {
        switch(current_state){
            case DriveState::Drive:
                current_state = executeDrive(path);
            case DriveState::TurnAroundObs:
                current_state = executeTurnAroundObs(path);
            case DriveState::DriveAroundObs:
                current_state = executeDriveAroundObs(path);
            case DriveState::Turn:
                current_state = executeTurn(path);
        }
    }
    path.clear();
}

DriveState SimplePathFollower::executeDrive(std::vector<Odometry>& path) {
    const Waypoint& nextWaypoint = path.front();
    double distance = estimateNoneuclid( gRover->roverStatus().odometry(), nextWaypoint.odom );

    if( isObstacleDetected( gRover ) && !isWaypointReachable( distance ) && isObstacleInThreshold( gRover, gRoverConfig ) )
    {
        mObstacleAvoidanceStateMachine->updateObstacleElements( getOptimalAvoidanceAngle(),
                                                                getOptimalAvoidanceDistance() );
        return DriveState::TurnAroundObs;
    }
    DriveStatus driveStatus = gRover->drive( nextWaypoint.odom );
    if( driveStatus == DriveStatus::Arrived )
    {
        gRover->roverStatus().course().pop_front();
        ++mCompletedWaypoints;
        return NavState::Done;
    }
    if( driveStatus == DriveStatus::OnCourse )
    {
        return NavState::Drive;
    }
    return NavState::Turn;
}

DriveState SimplePathFollower::executeTurn(std::vector<Odometry>& path) {
    if( gRover->roverStatus().course().empty() )
    {
        return DriveState::Done;
    }

    Odometry& nextPoint = path.front().odom;
    if( gRover->turn( nextPoint ) )
    {
        return DriveState::Drive;
    }
    return DriveState::Turn;
}

DriveState SimplePathFollower::executeTurnAroundObs(std::vector<Odometry>&  path) {
    if( !isObstacleDetected( gRover ) )
    {
        double distanceAroundObs = mOriginalObstacleDistance /
                                   cos( fabs( degreeToRadian( mOriginalObstacleAngle ) ) );
        mObstacleAvoidancePoint = createAvoidancePoint( gRover, distanceAroundObs );
        return DriveState::DriveAroundObs;
    }

    double obstacleBearing = gRover->roverStatus().obstacle().bearing;
    if( mJustDetectedObstacle &&
        ( obstacleBearing < 0 ? mLastObstacleAngle >= 0 : mLastObstacleAngle < 0 ) ) {
        obstacleBearing *= -1;
    }

    double desiredBearing = mod( gRover->roverStatus().odometry().bearing_deg + obstacleBearing, 360 );
    mJustDetectedObstacle = true;
    mLastObstacleAngle = obstacleBearing;
    gRover->turn( desiredBearing );
    return gRover->roverStatus().currentState();
}

DriveState SimplePathFollower::executeDriveAroundObs(std::vector<Odometry>& path) {
    if( isObstacleDetected( gRover )  && isObstacleInThreshold( gRover, gRoverConfig ) )
    {
        if( gRover->roverStatus().currentState() == DriveState::DriveAroundObs )
        {
            return DriveState::TurnAroundObs;
        }
    }

    DriveStatus driveStatus = gRover->drive( mObstacleAvoidancePoint );
    if( driveStatus == DriveStatus::Arrived )
    {
        return DriveState::Turn;
    }
    if( driveStatus == DriveStatus::OnCourse )
    {
        return gRover->roverStatus().currentState();
    }
    return DriveState::TurnAroundObs;

}

void SimplePathFollower::updateObstacleAngle(double bearing){
    mOriginalObstacleAngle = bearing;
}

void SimplePathFollower::updateObstacleDistance( double distance ) {
    mOriginalObstacleDistance = distance;
}

void SimplePathFollower::updateObstacleElements( double bearing, double distance ) {
    updateObstacleAngle( bearing );
    updateObstacleDistance( distance );
}

bool SimplePathFollower::isWaypointReachable( double distance ) {
    return isLocationReachable( mRover, mRoverConfig, distance, mRoverConfig["navThresholds"]["waypointDistance"].GetDouble());
}

Odometry SimplePathFollower::createAvoidancePoint( Rover* rover, const double distance ) {
    Odometry avoidancePoint = rover->roverStatus().odometry();
    double totalLatitudeMinutes = avoidancePoint.latitude_min +
                cos( degreeToRadian( avoidancePoint.bearing_deg ) ) * distance * LAT_METER_IN_MINUTES;
    double totalLongitudeMinutes = avoidancePoint.longitude_min +
                sin( degreeToRadian( avoidancePoint.bearing_deg ) ) * distance * rover->longMeterInMinutes();
    avoidancePoint.latitude_deg += totalLatitudeMinutes / 60;
    avoidancePoint.latitude_min = ( totalLatitudeMinutes - ( ( (int) totalLatitudeMinutes) / 60 ) * 60 );
    avoidancePoint.longitude_deg += totalLongitudeMinutes / 60;
    avoidancePoint.longitude_min = ( totalLongitudeMinutes - ( ( (int) totalLongitudeMinutes) / 60 ) * 60 );

    return avoidancePoint;
}
