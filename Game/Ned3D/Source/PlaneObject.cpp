/*
----o0o=================================================================o0o----
* Copyright (c) 2006, Ian Parberry
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the University of North Texas nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
----o0o=================================================================o0o----
*/

/// \file PlaneObject.cpp
/// \brief Code for the PlaneObject class.

/// Edited by the SAGE team, 2005
/// Last updated June 13, 2006

#define _USE_MATH_DEFINES

#include <assert.h>
#include <math.h>
#include "Common/MathUtil.h"
#include "PlaneObject.h"
#include "Input/Input.h"
#include "ObjectTypes.h"
#include "common/RotationMatrix.h"
#include "Ned3DObjectManager.h"
#include "Particle/ParticleEngine.h"
#include "Sound/SoundManager.h"
#include "game.h"
#include "directorymanager/directorymanager.h"

bool PlaneObject::takeDamage = true;

PlaneObject::PlaneObject(Model *m):
  ColorObject(m,2),
  m_gunPosition(0, 2.2f, 3.1f),
  m_enginePosition(0,0.0f, 3.3f),
  m_propOffset(0.4f),
  m_isPlaneAlive(true),
  m_maxTurnRate(kPi * 0.25f),
  m_maxPitchRate(kPi * 0.25f),
  m_maxBankRate(kPi * 0.25f),
  m_planeState(PS_FLYING),
  m_turnState(TS_STRAIGHT),
  m_pitchState(PS_LEVEL),
  m_moveState(MS_FORWARD),
  m_bankState(BS_STRAIGHT),
  m_hp(4),
  m_maxHP(4),
  m_minS(1.0f),
  m_maxS(5.0f),
  m_strafe(true),
  m_rollPressedLeft(false),
  m_rollPressedRight(false),
  m_rollTimer(0.0f),
  m_ePressed(false),
  m_headingState(HS_STRAIGHT),
  m_stopState(NO_STOP)
{
  assert(m);
  //assert(m->getPartCount() >= 2);
  setModelOrientation(EulerAngles(kPi, 0.0f, 0.0f));
  setPosition(0,m_propOffset,0);
  m_fSpeed = m_minS;
  m_maxSpeed = 2.0f;
  m_className = "Plane";
  m_type = ObjectTypes::PLANE;
  m_pitchRate = 0.0f;
  m_smokeID = -1;
  m_reticleLockOnUpdated = false;
  leftBarrelRoll = false;
  rightBarrelRoll = false;
  
  m_turnRate = 8.0f; // default for now

  m_timeSinceFired = gRenderer.getTime();

  // load gunshot sound
  gDirectoryManager.setDirectory(eDirectorySounds);
  m_gunSound = gSoundManager.load("Gun.wav",12);
  gSoundManager.setDistance(m_gunSound,50.0f,100000.0f);

  // load reticle texture
  m_reticleTexture = gRenderer.cacheTextureDX("reticle.png");

  // load all textures that will be used on the plane.
  // Multiple textures are used throughout game play so the plane appears to
  // "take damage"
  m_allTextures.resize(5);
  m_allTextures[0] = "plane2.tga"; // when hp = 0
  m_allTextures[1] = "plane2.tga"; // hp = 1
  m_allTextures[2] = "plane2.tga"; // hp = 2
  m_allTextures[3] = "plane2.tga"; // hp = 3
  m_allTextures[4] = "plane2.tga"; // hp > 3

  // cache all these
  for (int a =0; a < (int)m_allTextures.size(); a++)
    gRenderer.cacheTextureDX(m_allTextures[a].c_str());

  m_allParticles.resize(5);
  m_allParticles[0] = "smokeveryheavy";   // when hp = 0
  m_allParticles[1] = "smokeheavy";   // when hp = 0
  m_allParticles[2] = "smokemedium";  // hp = 1
  m_allParticles[3] = "smokelight";   // hp = 2
  m_allParticles[4] = "";             // hp > 2

  setTextureAndSmoke();

}

PlaneObject::~PlaneObject()
{
  // kill particle engine if one is attached
  if (m_smokeID != -1)
    gParticle.killSystem(m_smokeID);
  m_smokeID = -1;

}

void PlaneObject::renderReticle(float distance, float size)
{    
  
  // test terrain ray intersect
    Vector3 right = Vector3::kRightVector * size;
    Vector3 up = Vector3::kUpVector * size;
    
    Vector3 gunPos = m_gunPosition;
    Vector3 reticlePos = m_gunPosition;
    reticlePos.z += distance;

    Vector3 bulletDir = Vector3(0, 0, 1);    
        
    // set up a matrix to rotate points that are relative to the plane
    RotationMatrix r1;
    r1.setup(getOrientation());

    // transform points relative to the plane
    bulletDir = r1.objectToInertial(bulletDir);
    gunPos = r1.objectToInertial(gunPos);
    reticlePos = r1.objectToInertial(reticlePos);    
    right = r1.objectToInertial(right);
    up = r1.objectToInertial(up);

    // add position of the plane
    reticlePos += getPosition();
    gunPos += getPosition();

    // set up quad
    RenderVertexL verts[4];
    verts[0].p = reticlePos - right + up; 
    verts[0].u = 0.0f; verts[0].v = 0.0f;
    verts[1].p = reticlePos + right + up;
    verts[1].u = 1.0f; verts[1].v = 0.0f;
    verts[2].p = reticlePos + right - up;
    verts[2].u = 1.0f; verts[2].v = 1.0f;
    verts[3].p = reticlePos - right - up;
    verts[3].u = 0.0f; verts[3].v = 1.0f;
    
    
    // if we haven't already checked if the gun lines up with a enemy
    if (!m_reticleLockOnUpdated)
    {
      //TODO: Make sure this only happens once per process() call    
      m_reticleLockedOn = 
      gGame.m_statePlaying.m_objects->rayIntersectEnemy(gunPos,bulletDir * gBulletRange);
      m_reticleLockOnUpdated = true;
    }

    // set up color
    unsigned int color;
    if (m_reticleLockedOn)
    {      
      // make the color flash based on time
      int repeat = 600;
      int time = gRenderer.getTime() % repeat;
      float intensity = (sin( (float)time / (float)repeat * k2Pi) + 1.0f) / 2.0f;
      color = MAKE_ARGB(200.0f * intensity + 55.0f,255,0,0);
    }
    else color = MAKE_ARGB(255,0,180,0);

    for (int a = 0; a < 4; a++) verts[a].argb = color;

    // render reticle
    gRenderer.selectTexture(m_reticleTexture);
    gRenderer.renderQuad(verts);
}

void PlaneObject::process(float dt)
{
  if(!m_isPlaneAlive) return;
  // save the position
  m_oldPosition = getPosition();

  // Put any non-movement logic here

  if (m_rollPressedLeft) {
	  m_rollTimer += dt;
	  if (m_rollTimer > 0.6f) { // reset timer, too long between presses (.6 seconds default)
		  m_rollPressedLeft = false;
		  leftBarrelRoll = false;
		  m_rollTimer = 0.0f;
	  }
  }

    if (m_rollPressedRight) {
	  m_rollTimer += dt;
	  if (m_rollTimer > 0.6f) { // reset timer, too long between presses (.6 seconds default)
		  m_rollPressedRight = false;
		  rightBarrelRoll = false;
		  m_rollTimer = 0.0f;
	  }
  }

  // Unbuffered input (remove if handling input elsewhere)  
  inputStraight();
  inputLevel();
  inputStop();
 
  if (gInput.keyDown(DIK_A))  
    inputTurnLeft(4.0f);
  if (gInput.keyDown(DIK_D)) 
    inputTurnRight(4.0f);
  if (gInput.keyDown(DIK_S))  
    inputDive(1.0f);
  if (gInput.keyDown(DIK_W))  
    inputClimb(1.0f);
   
  if (gInput.keyDown(DIK_UP))
    inputSpeedUp();
  else if (gInput.keyDown(DIK_RIGHT))
    inputSpeedDown();
  else
	  m_moveState = MS_FORWARD;

  if(gInput.keyDown(DIK_DOWN)) inputFire();

  if (gInput.keyJustDown(DIK_F)) {
	  m_color = ((Color)(m_color+1));
	  if (m_color > 2) {
	    m_color=RED;
      }
  }

  EulerAngles &planeOrient = m_eaOrient[0];
  if (m_bankState != BS_BARRELROLL) {
	  // if not doing a barrel roll
	  if (gInput.keyDown(DIK_E) && gInput.keyDown(DIK_Q)) {
		  m_bankState = BS_STRAIGHT;
	  } else if (gInput.keyDown(DIK_E)) {
		   m_bankState = BS_TURNING;
		  if(planeOrient.bank > kPi * -0.50f)
				  planeOrient.bank -= m_maxBankRate * m_turnRate * dt;
	  }else if (gInput.keyDown(DIK_Q)) {
		  m_bankState = BS_TURNING;
		  if(planeOrient.bank < kPi * 0.50f)
				  planeOrient.bank += m_maxBankRate * m_turnRate * dt; 
	  } else if (m_bankState != BS_BARRELROLL ) {
		  m_bankState = BS_STRAIGHT;
	  } else {//do nothing for now
	  }

  }


  if ( (gInput.keyDown(DIK_E) || gInput.keyDown(DIK_Q)) && m_bankState != BS_BARRELROLL) {
	  m_turnRate = 4.0f;
  }


  if (gInput.keyJustDown(DIK_E)) {
	  if (m_rollPressedRight && m_ePressed) {
		  // pressed twice now, "Do a barrel roll!"
		  m_bankState = BS_BARRELROLL;
		  m_turnRate = 8.0f;
		  m_rollPressedRight = false;
	  } else {
		m_rollPressedRight = true;  // first time pressed in a while, so set first press to true and start timer
	  }
	  m_ePressed = true;
  }

    if (gInput.keyJustDown(DIK_Q)) {
	  if (m_rollPressedLeft && !m_ePressed) {
		  // pressed twice now, "Do a barrel roll!"
		  m_bankState = BS_BARRELROLL;
		  m_turnRate = -8.0f;
		  m_rollPressedLeft = false;
	  } else {
		m_rollPressedLeft = true;  // first time pressed in a while, so set first press to true and start timer
	  }
	  m_ePressed = false;
  }

  //if (gInput.keyDown(DIK_UP))
  inputForward(1.0f); // gonna always be going forward!

	// Xbox 360 controller functions.  If the controller is active,
	// run these control checks.
	if (gInput.Player1->IsConnected() == true)
	{
		// Left analog stick movement controls
		if (gInput.XInputPositionX() < 0.0f)
		  inputTurnLeft((-gInput.XInputPositionX() / 32768) * 4.0f);
		if (gInput.XInputPositionX() > 0.0f)
		  inputTurnRight((gInput.XInputPositionX() / 32768) * 4.0f);
		if (gInput.XInputPositionY() < 0.0f)
		  inputDive(-gInput.XInputPositionY() / 32768);
		if (gInput.XInputPositionY() > 0.0f)
			inputClimb(gInput.XInputPositionY() / 32768);

		// Shoot the gun
		if(gInput.XInputButtonDown(XINPUT_GAMEPAD_A))
			inputFire();

		// Break and boost functions
		if (gInput.XInputButtonDown(XINPUT_GAMEPAD_Y))
			inputSpeedUp();
		else if (gInput.XInputButtonDown(XINPUT_GAMEPAD_B))
			inputSpeedDown();
		else
		  m_moveState = MS_FORWARD;

		// Roll and barrel roll trigger controls
		if ((gInput.XInputLeftTrigger() > 0.0f || gInput.XInputRightTrigger() > 0.0f) && m_bankState != BS_BARRELROLL)
		{
			if (gInput.XInputLeftTrigger() > 0.0f)
			{
				m_turnRate = (gInput.XInputLeftTrigger() / 255) * 4.0f;
			}
			else if (gInput.XInputRightTrigger() > 0.0f)
			{
				m_turnRate = (gInput.XInputRightTrigger() / 255) * 4.0f;
			}
		}

		if (m_rollPressedLeft && (gInput.XInputLeftTrigger() < 255))
		{
			leftBarrelRoll = true;
		}
		
		if (gInput.XInputLeftTrigger() == 255)
		{
			if ((m_rollPressedLeft && !m_ePressed) && leftBarrelRoll)
			{
				m_bankState = BS_BARRELROLL;
				m_turnRate = -8.0f;
				m_rollPressedLeft = false;
				leftBarrelRoll = false;
			}
			else
			{
				m_rollPressedLeft = true;
			}
			m_ePressed = false;
		}

		if (m_rollPressedRight && (gInput.XInputRightTrigger() < 255))
		{
			rightBarrelRoll = true;
		}

		if (gInput.XInputRightTrigger() == 255)
		{
			if ((m_rollPressedRight && m_ePressed) && rightBarrelRoll)
			{
				m_bankState = BS_BARRELROLL;
				m_turnRate = 8.0f;
				m_rollPressedRight = false;
				rightBarrelRoll = false;
			}
			else
			{
				m_rollPressedRight = true;
			}
			m_ePressed = true;
		}

		if (m_bankState != BS_BARRELROLL)
		{
			if (gInput.XInputLeftTrigger() > 0.0f && gInput.XInputRightTrigger() == 0.0f)
			{
				m_bankState = BS_TURNING;
				if(planeOrient.bank < kPi * 0.50f)
					planeOrient.bank += m_maxBankRate * m_turnRate * dt;
			}
			else if (gInput.XInputRightTrigger() > 0.0f && gInput.XInputLeftTrigger() == 0.0f)
			{
				m_bankState = BS_TURNING;
				if(planeOrient.bank > kPi * -0.50f)
					planeOrient.bank -= m_maxBankRate * m_turnRate * dt;
			}
			else
				m_bankState = BS_STRAIGHT;
		}
  }

  else
  {
	  // process all joystick input if it is disabled
	  if (gInput.joyEnabled())
	  {	
		if(gInput.joyPadPositionX() < 0.0f)  
		  inputTurnLeft(-gInput.joyPadPositionX());  
		if(gInput.joyPadPositionX() > 0.0f)
		  inputTurnRight(gInput.joyPadPositionX());
		if(gInput.joyPadPositionY() > 0.0f)
		  inputDive(gInput.joyPadPositionY());
		if(gInput.joyPadPositionY() < 0.0f)
		  inputClimb(-gInput.joyPadPositionY());

		if (gInput.joyButtonDown(0)) inputFire();
	  }
  }

  if (m_planeState == PS_FLYING)
    m_velocity = (getPosition() - m_oldPosition)/dt;


  ColorObject::process(dt);
}

void PlaneObject::move(float dt)
{
	float tr =  0.02f;
  if(!m_isPlaneAlive)
    return;

  EulerAngles &planeOrient = m_eaOrient[0];
  Vector3 displacement = Vector3::kZeroVector;
  
  if (m_planeState == PS_FLYING)
  {

	  float turnSpeed = 0.0f;
	switch(m_moveState)
	{
		case MS_STOP:
		{
		turnSpeed = m_minS*1.5f; // min speed is default for bad states
		} break;
		case MS_FORWARD:
		{
		turnSpeed = m_maxS/2;
		} break;
		case MS_BOOST:
		{
			turnSpeed = m_maxS; // min speed is default for bad states
		} break;
	}
    // Update plane orientation
    if (m_strafe) {  // no turning, just strafe
		switch(m_turnState)
		{
			
		  case TS_LEFT:
		  {
			  if (m_bAllRange) {
				planeOrient.heading -= m_maxTurnRate * 1.0f * dt;
			  } else {
				m_fSpeedLeft =turnSpeed/2; // magic ratio of 2
			  }
			if(planeOrient.bank < kPi * 0.25f && m_bankState == BS_STRAIGHT)
			  planeOrient.bank += m_maxBankRate * m_turnRate * dt; 
			
		  } break;
		  case TS_RIGHT:
		  {
			  if (m_bAllRange) {
				planeOrient.heading += m_maxTurnRate * 1.0f * dt;\
			  } else {
				m_fSpeedRight =turnSpeed/2; // magic ratio of 2 
			  }
			if(planeOrient.bank > kPi * -0.25f && m_bankState == BS_STRAIGHT)
			  planeOrient.bank -= m_maxBankRate * m_turnRate * dt;
			
		  } break;
		  default:
		  {
			  m_fSpeedRight=m_fSpeedLeft=0.0f;
		  }
		};
	} else {                    // turn in all range mode
		switch(m_turnState)
		{
		  case TS_LEFT:
		  {

			planeOrient.heading -= m_maxTurnRate * m_turnRate * dt; //Old turn
			if(planeOrient.bank < kPi * 0.25f)
			  planeOrient.bank += m_maxBankRate * m_turnRate * dt;
		  } break;
		  case TS_RIGHT:
		  {
		
			planeOrient.heading += m_maxTurnRate * m_turnRate * dt; //Old turn
			if(planeOrient.bank > kPi * -0.25f)
			  planeOrient.bank -= m_maxBankRate * m_turnRate * dt; 
		  }
		};
	}
    
    switch(m_pitchState)
    {
      case PS_CLIMB:
      {
        planeOrient.pitch += m_pitchRate * m_maxPitchRate * dt;
      } break;
      case PS_DIVE:
      {
        planeOrient.pitch -= m_pitchRate * m_maxPitchRate * dt;
      }
    }
    clamp(planeOrient.pitch,-kPi * 0.125f,kPi * 0.125f);
    float bankCorrect = 4.0f * m_maxBankRate * dt;
    float pitchCorrect = 1.0f * m_maxPitchRate * dt;

	if (m_bankState == BS_BARRELROLL) {
		if(planeOrient.bank > kPi * -2.0f && planeOrient.bank < kPi * 2.0f)
			  planeOrient.bank -= m_maxBankRate * m_turnRate * dt;
		else {
			planeOrient.bank = 0.0f;
			m_bankState = BS_STRAIGHT;
		}
	} else {
		m_turnRate = 4.0f;
		if(m_turnState == TS_STRAIGHT && m_bankState == BS_STRAIGHT)
		  if(planeOrient.bank > bankCorrect)
			planeOrient.bank -= bankCorrect;
		  else if(planeOrient.bank < -bankCorrect)
			planeOrient.bank += bankCorrect;
		  else planeOrient.bank = 0.0f;

	}
    if(m_pitchState == PS_LEVEL)
      if(planeOrient.pitch > pitchCorrect)
        planeOrient.pitch -= pitchCorrect;
      else if(planeOrient.pitch < -pitchCorrect)
        planeOrient.pitch += pitchCorrect;
      else planeOrient.pitch = 0.0f;

	  
	if (!m_bAllRange) {
		switch(m_headingState) {
		case HS_STRAIGHT:
			if (planeOrient.heading > 0.03f) {
				planeOrient.heading -= tr;
			} else if (planeOrient.heading < -0.03f) {
				planeOrient.heading += tr;
			} else {
				planeOrient.heading = 0.0f;
			}
			break;
		case HS_LEFT:
			if (planeOrient.heading + (float)M_PI/2 > 0.03f) {
				planeOrient.heading -= tr;
			} else if (planeOrient.heading + (float)M_PI/2 < -0.03f) {
				planeOrient.heading += tr;
			} else {
				planeOrient.heading = (float)-M_PI/2;
			}
			break;
		case HS_RIGHT:
			if (planeOrient.heading - (float)M_PI/2 > 0.03f) {
				planeOrient.heading -= tr;
			} else if (planeOrient.heading - (float)M_PI/2 < -0.03f) {
				planeOrient.heading += tr;
			} else {
				planeOrient.heading = (float)M_PI/2;
			}
			break;
		}
	}
    
    // Check move state
    
    switch(m_moveState)
    {
      case MS_STOP:
      {
        m_fSpeed = m_minS; // min speed is default for bad states
      } break;
      case MS_FORWARD:
      {
        m_fSpeed = m_maxS/2;
      } break;
      case MS_BOOST:
      {
         m_fSpeed = m_maxS*2; // min speed is default for bad states
      } break;
    }
  } // end if (m_planeState == PS_FLYING)
  if (m_planeState == PS_CRASHING)
  {
      m_eaAngularVelocity[0].bank = 1.0f;
      Vector3 normVelocity = m_velocity;
      normVelocity.normalize();
      m_eaOrient[0].pitch = -(float)asin((double)(normVelocity.y));
      float gravity =  -10.0f;
      
      m_fSpeed = 0.0f;
      m_velocity.y += gravity * dt;      
      m_v3Position[0] += m_velocity * dt;  
  }

  // Update propeller orientation

  m_eaOrient[1].bank += kPi * dt * 4.0f;
  

  // Stop movement if up against a wall
  if (m_bAllRange) {

  } else {
	  if ( (STOP_LEFT & m_stopState) == STOP_LEFT ) {
		  switch(m_headingState) {
			case HS_STRAIGHT:
				m_fSpeedLeft = 0.0f;
				break;
			case HS_LEFT:
				//m_fSpeed = 0.0f;
				break;
			case HS_RIGHT:
				break;
		  }
	  }
		  
	  if ( (STOP_RIGHT & m_stopState) == STOP_RIGHT ) {
		  switch(m_headingState) {
			case HS_STRAIGHT:
				m_fSpeedRight = 0.0f;
				break;
			case HS_LEFT:
				break;
			case HS_RIGHT:
				//m_fSpeed = 0.0f;
				break;
		  }
	  }
		
	  if ( (STOP_UP & m_stopState) == STOP_UP ) {
		  if (m_eaOrient->pitch < 0) {
			  m_eaOrient->pitch = 0.0f;
			  m_eaAngularVelocity[0].pitch = 0.0f;
		  }
		  
	  }

	  if ( (STOP_DOWN & m_stopState) == STOP_DOWN ) {
		  if (m_eaOrient->pitch > 0) {
			  m_eaOrient->pitch = 0.0f;
			  m_eaAngularVelocity[0].pitch = 0.0f;
		  }
		  
	  }

	  if ( (STOP_NEAR & m_stopState) == STOP_NEAR ) {
		  switch(m_headingState) {
			case HS_STRAIGHT:
				//m_fSpeed = 0.0f;
				break;
			case HS_LEFT:
				m_fSpeedLeft = 0.0f;
				break;
			case HS_RIGHT:
				m_fSpeedRight = 0.0f;
				break;
		  }
		  
		  
	  }

	  if ( (STOP_FAR & m_stopState) == STOP_FAR ) {
		  switch(m_headingState) {
			case HS_STRAIGHT:
				
				break;
			case HS_LEFT:
				m_fSpeedRight = 0.0f;
				break;
			case HS_RIGHT:
				m_fSpeedLeft = 0.0f;
				break;
		  }
		  
	  }
  }



  // Move it
  
  GameObject::move(dt);

  // flag that m_reticleLockedOn is out of date
  m_reticleLockOnUpdated = false; 

  // tell the smoke particle system where the plane is if there is smoke
  if (m_smokeID != -1)
    gParticle.setSystemPos(m_smokeID, transformObjectToInertial(m_enginePosition));

  
  
}

void PlaneObject::reset()
{
  m_isPlaneAlive = true;
  m_v3Position[0].zero(); // collision handling will fix height
  m_eaOrient[0].identity();
  m_eaOrient[1].identity();
  m_fSpeed = m_minS*2;
  m_maxSpeed = m_minS;
  m_turnState = TS_STRAIGHT;
  m_pitchState = PS_LEVEL;
  m_moveState = MS_FORWARD;
}

void PlaneObject::inputTurnLeft(float turnRatio)
{
  //m_turnRate = turnRatio;
  m_turnState = m_turnState == TS_RIGHT ? TS_STRAIGHT : TS_LEFT;
}

void PlaneObject::inputTurnRight(float turnRatio)
{
  //m_turnRate = turnRatio;
  m_turnState = m_turnState == TS_LEFT ? TS_STRAIGHT : TS_RIGHT;
}

void PlaneObject::inputStraight()
{
  m_turnState = TS_STRAIGHT;
}

void PlaneObject::inputClimb(float climbRatio)
{
  m_pitchRate = climbRatio;
  m_pitchState = m_pitchState == PS_DIVE ? PS_LEVEL : PS_CLIMB;
}

void PlaneObject::inputDive(float diveRatio)
{
  m_pitchRate = diveRatio;
  m_pitchState = m_pitchState == PS_CLIMB ? PS_LEVEL : PS_DIVE;
}

void PlaneObject::inputLevel()
{
  m_pitchState = PS_LEVEL;
}

void PlaneObject::inputSpeedDown()
{
	m_moveState = MS_STOP;
	/*
  m_maxSpeed -= 1.0f;
  if (m_minS > m_maxSpeed) {
	  m_maxSpeed = m_minS;
  } else if (m_maxS < m_maxSpeed) {
	  m_maxSpeed = m_maxS;
  }*/
}

void PlaneObject::inputSpeedUp()
{
	m_moveState = MS_BOOST;
	/*
  m_maxSpeed += 1.0f;
  if (m_minS > m_maxSpeed) {
	  m_maxSpeed = m_minS;
  } else if (m_maxS < m_maxSpeed) {
	  m_maxSpeed = m_maxS;
  }*/
}

void PlaneObject::inputForward(float speed)
{
  m_speedRatio = speed;
}

void PlaneObject::inputBackward()
{
  //m_moveState = m_moveState == MS_FORWARD ? MS_STOP : MS_BOOST;
}

void PlaneObject::inputStop()
{
  m_moveState = MS_STOP;
}

void PlaneObject::inputFire()
{
  const float BulletsPerSecond = 10.0f;
  
  int time = gRenderer.getTime();
  if ((float)(time - m_timeSinceFired)/1000.0f < 1.0f/BulletsPerSecond)
    return;

  m_timeSinceFired = time;
  
  // test terrain ray intersect
  Vector3 bulletPos = getPosition();
  Vector3 bulletDir = Vector3(0, 0, 1);
  Vector3 gunPos = m_gunPosition;

  RotationMatrix r1;
  r1.setup(getOrientation());
  
  bulletDir = r1.objectToInertial(bulletDir);
  gunPos.z += 5.0f;
  gunPos = r1.objectToInertial(gunPos);
  gunPos += getPosition();
  Vector3 intersectPoint = Vector3::kZeroVector;
  
  unsigned int bulletID = gGame.m_statePlaying.m_objects->spawnBullet(gunPos,getOrientation(),m_color);
  unsigned int mfID = gParticle.createSystem("muzzlefire");
  //gParticle.setSystemPos(mfID, gGame.m_statePlaying.m_objects->getObjectPointer(bulletID)->getPosition());

  int gunSoundInstance = gSoundManager.requestInstance(m_gunSound);
  gSoundManager.setPosition(m_gunSound, gunSoundInstance, getPosition());
  gSoundManager.setVelocity(m_gunSound, gunSoundInstance, -m_velocity );
  gSoundManager.play(m_gunSound, gunSoundInstance);
  gSoundManager.releaseInstance(m_gunSound, gunSoundInstance);

  // get ray trace up' and right'
  Vector3 v1 = bulletDir;
  v1.z = (10.0f + v1.z) * 20.0f; // pick a vector that is not bulletDir
  Vector3 up = Vector3::crossProduct(bulletDir, v1);
  up.normalize();
  float traceWidth = 0.2f;
  up *= traceWidth;

  Vector3 right = Vector3::crossProduct(bulletDir, up);
  right.normalize();
  right *= traceWidth;

  Vector3 traceVector = bulletDir * gBulletRange;

  if(gGame.m_statePlaying.terrain->rayIntersect(gunPos, traceVector, intersectPoint))
  {
    if(intersectPoint.y > gGame.m_statePlaying.water->getWaterHeight())
    {
      int tmpHndl = gParticle.createSystem("bulletdust");
      //gParticle.setSystemPos(tmpHndl, intersectPoint);
    }
    else
    {
      float dy = bulletPos.y - gGame.m_statePlaying.water->getWaterHeight();
      if(bulletDir.y < 0)
        dy *= -1.0f;
      intersectPoint = bulletPos + (bulletDir * (dy / bulletDir.y));

      int tmpHndl = gParticle.createSystem("bulletspray");
      //gParticle.setSystemPos(tmpHndl, intersectPoint);
    }
  }

}

void PlaneObject::killPlane()
{
  m_planeState = PS_DEAD;
  m_isPlaneAlive = false;
}

bool PlaneObject::isPlaneAlive() const
{
  return m_isPlaneAlive;
}

int PlaneObject::getHP() const
{
  return m_hp;
}

int PlaneObject::getMaxHP() const
{
  return m_maxHP;
}

void PlaneObject::setMaxHP(int maxHP, bool recoverAll)
{
  if(maxHP <= 0)
    return;
  m_maxHP = maxHP;
  if(recoverAll)
    this->recoverAll(); // the function, not the parameter
}

void PlaneObject::damage(int hp)
{
  // if god mode is on leave 
  if (!takeDamage)
    return;

  m_hp -= hp;
  setTextureAndSmoke(); // change to a texture with more damange on it
        
  if(m_isPlaneAlive && m_hp <= 0)
  {
    m_planeState = PS_CRASHING;
    m_velocity = Vector3::kForwardVector;
    m_eaOrient[0].pitch = degToRad(20);
    RotationMatrix r; r.setup(m_eaOrient[0]);
    m_velocity = r.objectToInertial(m_velocity);
    m_velocity *= m_maxSpeed * m_speedRatio * 20.0f;
  }
}

void PlaneObject::recover(int hp)
{
  m_hp += hp;
  if(m_hp > m_maxHP)
    m_hp = m_maxHP;
}

void PlaneObject::recoverAll()
{
  m_hp = m_maxHP;
}

void PlaneObject::setTextureAndSmoke()
{
  
  int textureIndex = m_hp; // index into m_allTextures array
  int smokeIndex = m_hp; // index into m_allParticles array
  
  // make sure the indicies are in range
  if (textureIndex >= (int)m_allTextures.size())
    textureIndex = (int)m_allTextures.size() - 1;
  if (textureIndex < 0) textureIndex = 0;

  // make sure the indicies are in range
  if (smokeIndex >= (int)m_allParticles.size())
    smokeIndex = (int)m_allParticles.size() - 1;
  if (smokeIndex < 0) smokeIndex = 0;

    // set texture 
  int numParts = m_pModel->getPartCount();
  for (int a = 0; a < numParts; a++)
    m_pModel->setPartTextureName(a,m_allTextures[textureIndex].c_str());
   m_pModel->cache();

   // remove previous smoke system
   if (m_smokeID != -1)
     gParticle.killSystem(m_smokeID);
   if (m_allParticles[smokeIndex] != "")
   {
     m_smokeID = gParticle.createSystem(m_allParticles[smokeIndex]);
     gParticle.setSystemPos(m_smokeID, transformObjectToInertial(m_enginePosition));
   }
}