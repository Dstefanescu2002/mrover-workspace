#include "rover.hpp"

#include "utilities.hpp"
#include "rover_msgs/Joystick.hpp"
#include <cmath>
#include <iostream>

// Constructs a rover status object and initializes the navigation
// state to off.
Rover::RoverStatus::RoverStatus()
{
    this->mAutonState.is_auton = false;
} // RoverStatus()

// Updates the auton state (on/off) of the rover's status.
void Rover::RoverStatus::updateRoverStatus( AutonState autonState )
{
    Rover::RoverStatus::autonState() = autonState;
} // updateRoverStatus( AutonState )

// Updates the course of the rover's status if it has changed.
void Rover::RoverStatus::updateRoverStatus( Destinations destinations )
{
    if( Rover::RoverStatus::destinations().hash != destinations.hash )
    {
        Rover::RoverStatus::destinations() = destinations;
        for (int i = 0; i < destinations.num_waypoints; ++i) {
            Rover::RoverStatus::course().push_back(destinations.waypoints[i]);
        }
    }
} // updateRoverStatus( Course )

// Updates the obstacle information of the rover's status.
void Rover::RoverStatus::updateRoverStatus( Obstacle obstacle )
{
    Rover::RoverStatus::obstacle() = obstacle;
} // updateRoverStatus( Obstacle )

// Updates the odometry information of the rover's status.
void Rover::RoverStatus::updateRoverStatus( Odometry odometry )
{
    Rover::RoverStatus::odometry() = odometry;
} // updateRoverStatus( Odometry )

// Updates the target information of the rover's status.
void Rover::RoverStatus::updateRoverStatus( TargetList targetList )
{
    Target target1 = targetList.targetList[0];
    Target target2 = targetList.targetList[1];
    Rover::RoverStatus::target() = target1;
    Rover::RoverStatus::target2() = target2;
} // updateRoverStatus( Target )

// Updates the radio signal strength information of the rover's status.
void Rover::RoverStatus::updateRoverStatus( RadioSignalStrength radioSignalStrength )
{
    radio() = radioSignalStrength;
} // updateRoverStatus( RadioSignalStrength )


// Gets a reference to the rover's destinations.
Destinations& Rover::RoverStatus::destinations()
{
    return mDestinations;
} // destinations()


//Gets a reference to the rovers Course
deque<Waypoint>& Rover::RoverStatus::course(){
    return mCourse;
}


// Gets a reference to the rover's current odometry information.
Odometry& Rover::RoverStatus::odometry()
{
    return mOdometry;
} // odometry()

// Gets a reference to the rover's first target's current information.
Target& Rover::RoverStatus::target()
{
    return mTarget1;
} // target()

Target& Rover::RoverStatus::target2() {
    return mTarget2;
}

bool Rover::RoverStatus::firstGatePostFound() {
    return mFirstGatePostFound;
}

PostLocation& Rover::RoverStatus::post1(){
    return mPost1;
}

PostLocation& Rover::RoverStatus::post2(){
    return mPost2;
}

vector<Odometry>& Rover::RoverStatus::path(){
    return mPath;
}

RadioSignalStrength& Rover::RoverStatus::radio() {
    return mSignal;
}

const rapidjson::Document& Rover::RoverConfig(){
    return mRoverConfig;
}

// Gets a reference to the rover's current obstacle information.
Obstacle& Rover::RoverStatus::obstacle()
{
    return mObstacle;
} // obstacle()

AutonState& Rover::RoverStatus::autonState()
{
    return mAutonState;
}


// Assignment operator for the rover status object. Does a "deep" copy
// where necessary.
Rover::RoverStatus& Rover::RoverStatus::operator=( Rover::RoverStatus& newRoverStatus )
{
    mDestinations = newRoverStatus.destinations();
    mOdometry = newRoverStatus.odometry();
    mTarget1 = newRoverStatus.target();
    mTarget2 = newRoverStatus.target2();
    mSignal = newRoverStatus.radio();
    return *this;
} // operator=

// Constructs a rover object with the given configuration file and lcm
// object with which to use for communications.
Rover::Rover( const rapidjson::Document& config, lcm::LCM& lcmObject )
    : mRoverConfig( config )
    , mLcmObject( lcmObject )
    , mDistancePid( config[ "distancePid" ][ "kP" ].GetDouble(),
                    config[ "distancePid" ][ "kI" ].GetDouble(),
                    config[ "distancePid" ][ "kD" ].GetDouble() )
    , mBearingPid( config[ "bearingPid" ][ "kP" ].GetDouble(),
                   config[ "bearingPid" ][ "kI" ].GetDouble(),
                   config[ "bearingPid" ][ "kD" ].GetDouble() )
    , mTimeToDropRepeater( false )
    , mLongMeterInMinutes( -1 )
    , mGimbal( config["gimbal"]["tolerance"].GetDouble() )
    , mGimbalAngles( 0,1)
{
    //TODO: init mGimbalAngles from config array (use for loop)
    //TODO: check this compiles
    //double angles[] = config["gimbal"]["angles1"].GetArray();
    //int n = sizeof(angles) / sizeof(angles[0]);
    //mGimbalAngles(angles, angles + n);

} // Rover()

// Sends a joystick command to drive forward from the current odometry
// to the destination odometry. This joystick command will also turn
// the rover small amounts as "course corrections".
// The return value indicates if the rover has arrived or if it is
// on-course or off-course.
DriveStatus Rover::drive( const Odometry& destination )
{
    double distance = estimateNoneuclid( mRoverStatus.odometry(), destination );
    double bearing = calcBearing( mRoverStatus.odometry(), destination );
    return drive( distance, bearing, false );
} // drive()

// Sends a joystick command to drive forward from the current odometry
// in the direction of bearing. The distance is used to determine how
// quickly to drive forward. This joystick command will also turn the
// rover small amounts as "course corrections". target indicates
// if the rover is driving to a target rather than a waypoint and
// determines which distance threshold to use.
// The return value indicates if the rover has arrived or if it is
// on-course or off-course.
DriveStatus Rover::drive( const double distance, const double bearing, const bool target )
{
    if( (!target && distance < mRoverConfig[ "navThresholds" ][ "waypointDistance" ].GetDouble()) ||
        (target && distance < mRoverConfig[ "navThresholds" ][ "targetDistance" ].GetDouble()) )
    {
        return DriveStatus::Arrived;
    }

    double destinationBearing = mod( bearing, 360 );
    throughZero( destinationBearing, mRoverStatus.odometry().bearing_deg ); // will go off course if inside if because through zero not calculated

    if( fabs( destinationBearing - mRoverStatus.odometry().bearing_deg ) < mRoverConfig[ "navThresholds" ][ "drivingBearing" ].GetDouble() )
    {
        double distanceEffort = mDistancePid.update( -1 * distance, 0 );
        double turningEffort = mBearingPid.update( mRoverStatus.odometry().bearing_deg, destinationBearing );
        publishJoystick( distanceEffort, turningEffort, false );
        return DriveStatus::OnCourse;
    }
    cerr << "offcourse\n";
    return DriveStatus::OffCourse;
} // drive()

// Sends a joystick command to drive in the given direction and to
// turn in the direction of bearing. Note that this version of drive
// does not calculate if you have arrive at a specific location and
// this must be handled outside of this function.
// The input bearing is an absolute bearing.
void Rover::drive(const int direction, const double bearing)
{
    double destinationBearing = mod(bearing, 360);
    throughZero(destinationBearing, mRoverStatus.odometry().bearing_deg);
    const double distanceEffort = mDistancePid.update(-1 * direction, 0);
    const double turningEffort = mBearingPid.update(mRoverStatus.odometry().bearing_deg, destinationBearing);
    publishJoystick(distanceEffort, turningEffort, false);
} // drive()

// Sends a joystick command to turn the rover toward the destination
// odometry. Returns true if the rover has finished turning, false
// otherwise.
bool Rover::turn( Odometry& destination )
{
    double bearing = calcBearing( mRoverStatus.odometry(), destination );
    return turn( bearing );
} // turn()

// Sends a joystick command to turn the rover. The bearing is the
// absolute bearing. Returns true if the rover has finished turning, false
// otherwise.
bool Rover::turn( double bearing )
{
    bearing = mod(bearing, 360);
    throughZero( bearing, mRoverStatus.odometry().bearing_deg );
    double turningBearingThreshold;
    if( isTurningAroundObstacle() )
    {
        turningBearingThreshold = 0;
    }
    else
    {
        turningBearingThreshold = mRoverConfig[ "navThresholds" ][ "turningBearing" ].GetDouble();
    }
    if( fabs( bearing - mRoverStatus.odometry().bearing_deg ) <= turningBearingThreshold )
    {
        return true;
    }
    double turningEffort = mBearingPid.update( mRoverStatus.odometry().bearing_deg, bearing );
    double minTurningEffort = mRoverConfig[ "navThresholds" ][ "minTurningEffort" ].GetDouble() * (turningEffort < 0 ? -1 : 1);
    if( isTurningAroundObstacle() && fabs(turningEffort) < minTurningEffort )
    {
        turningEffort = minTurningEffort;
    }
    publishJoystick( 0, turningEffort, false );
    return false;
} // turn()

// Sends a joystick command to stop the rover.
void Rover::stop()
{
    publishJoystick( 0, 0, false );
} // stop()


// Checks if the rover should be updated based on what information in
// the rover's status has changed. Returns true if the rover was
// updated, false otherwise.
// TODO: Remove this function if we don't run into any problems without it.
// It is very out dated, so if used we have to remake it
bool Rover::updateRover( RoverStatus newRoverStatus )
{
    // Rover currently on.
    if( mRoverStatus.autonState().is_auton )
    {
        // Rover turned off
        if( !newRoverStatus.autonState().is_auton )
        {
            mRoverStatus.autonState() = newRoverStatus.autonState();
            return true;
        }

        // If any data has changed, update all data
        if( !isEqual( mRoverStatus.odometry(), newRoverStatus.odometry() ) ||
            !isEqual( mRoverStatus.target(), newRoverStatus.target()) ||
            !isEqual( mRoverStatus.target2(), newRoverStatus.target2()) )
        {
            mRoverStatus.obstacle() = newRoverStatus.obstacle();
            mRoverStatus.odometry() = newRoverStatus.odometry();
            mRoverStatus.target() = newRoverStatus.target();
            mRoverStatus.radio() = newRoverStatus.radio();
            updateRepeater(mRoverStatus.radio());
            return true;
        }

        return false;
    }

    // Rover currently off.
    else
    {
        // Rover turned on.
        if( newRoverStatus.autonState().is_auton )
        {
            mRoverStatus = newRoverStatus;
            // Calculate longitude minutes/meter conversion.
            mLongMeterInMinutes = 60 / ( EARTH_CIRCUM * cos( degreeToRadian(
                mRoverStatus.odometry().latitude_deg, mRoverStatus.odometry().latitude_min ) ) / 360 );
            return true;
        }
        return false;
    }
} // updateRover()

// Calculates the conversion from minutes to meters based on the
// rover's current latitude.
const double Rover::longMeterInMinutes() const
{
    return mLongMeterInMinutes;
}

const vector<double> Rover::gimbalAngles() const
{
    return mGimbalAngles;
}

int& Rover::gimbalIndex()
{
    return mGimbalIndex;
}

// Executes the logic starting the clock to time how long it's been
// since the rover has gotten a strong radio signal. If the signal drops
// below the signalStrengthCutOff and the timer hasn't started, begin the clock.
// Otherwise, the signal is good so the timer should be stopped.
void Rover::updateRepeater(RadioSignalStrength& radioSignal)
{
    static bool started = false;
    static time_t startTime;

    // If we haven't already dropped a repeater, the time hasn't already started
    // and our signal is below the threshold, start the timer
    if( !mTimeToDropRepeater &&
        !started &&
        radioSignal.signal_strength <=
        mRoverConfig[ "radioRepeaterThresholds" ][ "signalStrengthCutOff" ].GetDouble())
    {
        startTime = time( nullptr );
        started = true;
    }

    double waitTime = mRoverConfig[ "radioRepeaterThresholds" ][ "lowSignalWaitTime" ].GetDouble();
    if( started && difftime( time( nullptr ), startTime ) > waitTime )
    {
        started = false;
        mTimeToDropRepeater = true;
    }
}

// Returns whether or not enough time has passed to drop a radio repeater.
bool Rover::isTimeToDropRepeater()
{
    return mTimeToDropRepeater;
}

// Gets the rover's status object.
Rover::RoverStatus& Rover::roverStatus()
{
    return mRoverStatus;
} // roverStatus()

// Gets the rover's driving pid object.
PidLoop& Rover::distancePid()
{
    return mDistancePid;
} // distancePid()

// Gets the rover's turning pid object.
PidLoop& Rover::bearingPid()
{
    return mBearingPid;
} // bearingPid()

// Gets the rover's gimbal object
Gimbal& Rover::gimbal()
{
    return mGimbal;
}

// tells the gimbal to go to its requested location, DEPRECATED
void Rover::publishGimbal(){
    mGimbal.publishControlSignal( mLcmObject, mRoverConfig );
}

// sends the gimbal a desired yaw setpoint, gimbal publishes command
bool Rover::sendGimbalSetpoint(double desired_yaw){
    bool r = mGimbal.setDesiredGimbalYaw(desired_yaw);
    mGimbal.publishControlSignal( mLcmObject, mRoverConfig );
    return r;
}

// Publishes a joystick command with the given forwardBack and
// leftRight efforts.
void Rover::publishJoystick( const double forwardBack, const double leftRight, const bool kill )
{
    Joystick joystick;
    // power limit (0 = 50%, 1 = 0%, -1 = 100% power)
    joystick.dampen = mRoverConfig[ "joystick" ][ "dampen" ].GetDouble();
    double drivingPower = mRoverConfig[ "joystick" ][ "drivingPower" ].GetDouble();
    joystick.forward_back = drivingPower * forwardBack;
    double bearingPower = mRoverConfig[ "joystick" ][ "bearingPower" ].GetDouble();
    joystick.left_right = bearingPower * leftRight;
    joystick.kill = kill;
    string joystickChannel = mRoverConfig[ "lcmChannels" ][ "joystickChannel" ].GetString();
    mLcmObject.publish( joystickChannel, &joystick );
} // publishJoystick()

// Returns true if the two odometry messages are equal, false
// otherwise.
bool Rover::isEqual( const Odometry& odometry1, const Odometry& odometry2 ) const
{
    if( odometry1.latitude_deg == odometry2.latitude_deg &&
        odometry1.latitude_min == odometry2.latitude_min &&
        odometry1.longitude_deg == odometry2.longitude_deg &&
        odometry1.longitude_min == odometry2.longitude_min &&
        odometry1.bearing_deg == odometry2.bearing_deg )
    {
        return true;
    }
    return false;
} // isEqual( Odometry )

// Returns true if the two target messages are equal, false
// otherwise.
bool Rover::isEqual( const Target& target1, const Target& target2 ) const
{
    if( target1.distance == target2.distance &&
        target1.bearing == target2.bearing )
    {
        return true;
    }
    return false;
} // isEqual( Target )

// Return true if the current state is TurnAroundObs or SearchTurnAroundObs,
// false otherwise.
bool Rover::isTurningAroundObstacle() const
{
 ///TODO FILL THIS IN WITH A MEMBER VARIABLE
   return false;
} // isTurningAroundObstacle()


Rover* gRover;

/*************************************************************************/
/* TODOS */
/*************************************************************************/
