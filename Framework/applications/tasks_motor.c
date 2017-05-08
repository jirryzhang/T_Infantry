#include "tasks_motor.h"
#include "drivers_canmotor_user.h"
#include "rtos_semaphore.h"
#include "drivers_uartrc_user.h"
#include "utilities_debug.h"
#include "tasks_upper.h"
#include "tasks_cmcontrol.h"
#include "tasks_remotecontrol.h"
#include "drivers_led_user.h"
#include "utilities_minmax.h"
#include "pid_regulator.h"
#include "application_motorcontrol.h"
#include "drivers_sonar_user.h"
#include "usart.h"
#include "peripheral_define.h"
#include "drivers_uartupper_user.h"

#include "stdint.h"

//PID_INIT(Kp, Ki, Kd, KpMax, KiMax, KdMax, OutputMax)
fw_PID_Regulator_t pitchPositionPID = fw_PID_INIT(15.0, 2.0, 1.0, 10000.0, 10000.0, 10000.0, 10000.0);
fw_PID_Regulator_t yawPositionPID = fw_PID_INIT(5.3, -1.0, 0.5, 10000.0, 10000.0, 10000.0, 10000.0);//�ȷ���P37.3 I11.9 D3.75  ԭ26.1 8.0 1.1
fw_PID_Regulator_t pitchSpeedPID = fw_PID_INIT(25.0, 0.0, 5.0, 10000.0, 10000.0, 10000.0, 3500.0);
fw_PID_Regulator_t yawSpeedPID = fw_PID_INIT(40.0, 0.0, 20, 10000.0, 10000.0, 10000.0, 4000.0);
PID_Regulator_t CMRotatePID = CHASSIS_MOTOR_ROTATE_PID_DEFAULT; 
PID_Regulator_t CM1SpeedPID = CHASSIS_MOTOR_SPEED_PID_DEFAULT;
PID_Regulator_t CM2SpeedPID = CHASSIS_MOTOR_SPEED_PID_DEFAULT;
PID_Regulator_t CM3SpeedPID = CHASSIS_MOTOR_SPEED_PID_DEFAULT;
PID_Regulator_t CM4SpeedPID = CHASSIS_MOTOR_SPEED_PID_DEFAULT;

fw_PID_Regulator_t testPositionPID = fw_PID_INIT(6.0, 0.0, 0.0, 1000000.0, 1000000.0, 1000000.0, 1000000.0);
fw_PID_Regulator_t testSpeedPID = fw_PID_INIT(1.0, 0.0, 0.0, 10000.0, 10000.0, 10000.0, 4900.0);//0.0, 0.00003

fw_PID_Regulator_t platePositionPID = fw_PID_INIT(40.0, 0.0, 0.0, 1000000.0, 1000000.0, 1000000.0, 1000000.0);
fw_PID_Regulator_t plateSpeedPID = fw_PID_INIT(1.0, 0.0, 0.0, 10000.0, 10000.0, 10000.0, 4900.0);//0.0, 0.00003

extern float gYroXs, gYroYs, gYroZs;

extern PID_Regulator_t CMRotatePID;
extern volatile Encoder CM1Encoder;
extern volatile Encoder CM2Encoder;
extern volatile Encoder CM3Encoder;
extern volatile Encoder CM4Encoder;
extern volatile Encoder GMYawEncoder;
//extern int forPidDebug;

float pitchRealAngle = 0.0;
extern Location_Number_s Location_Number[];
//extern uint16_t pitchAngle, yawAngle;
//extern uint32_t flAngle, frAngle, blAngle, brAngle;
//extern uint16_t flSpeed, frSpeed, blSpeed, brSpeed;
extern uint8_t CReceive;
extern uint8_t rune_flag;
extern uint8_t GYRO_RESETED;
extern float ZGyroModuleAngle;
float yawAngleTarget = 0.0;
float pitchAngleTarget = 0.0;
int8_t flUpDown = 0, frUpDown = 0, blUpDown = 0, brUpDown = 0, allUpDown = 0;
void CMGMControlTask(void const * argument){
	static float yawAdd;
	static float pitchAdd;
	static uint8_t rune;
	while(1){
//  osSemaphoreWait(imurefreshGimbalSemaphoreHandle, osWaitForever);
		osSemaphoreWait(CMGMCanRefreshSemaphoreHandle, osWaitForever);
		if(IOPool_hasNextRead(upperIOPool, 0)){
		IOPool_getNextRead(upperIOPool, 0);
		yawAdd = IOPool_pGetReadData(upperIOPool, 0)->yawAdd;
		pitchAdd = IOPool_pGetReadData(upperIOPool, 0)->pitchAdd;
		rune = IOPool_pGetReadData(upperIOPool, 0)->rune;
		}
/*��̨yaw��*/
		if(IOPool_hasNextRead(GMYAWRxIOPool, 0)){
			
			uint16_t yawZeroAngle = 1075;
			float yawRealAngle = 0.0;
			int16_t yawIntensity = 0;
			
			IOPool_getNextRead(GMYAWRxIOPool, 0); 
			Motor820RRxMsg_t tempData; tempData.angle = IOPool_pGetReadData(GMYAWRxIOPool, 0)->angle;
			tempData.RotateSpeed = 0;
			CANReceiveMsgProcess_820R(&tempData, &GMYawEncoder);
/*���̸����������תPID����*/
		 CMRotatePID.ref = 2.0f;
		 CMRotatePID.fdb = GMYawEncoder.ecd_angle;
	   CMRotatePID.Calc(&CMRotatePID);   
		 ChassisSpeedRef.rotate_ref = CMRotatePID.output;
				 ChassisSpeedRef.rotate_ref = 0;
			yawRealAngle = (IOPool_pGetReadData(GMYAWRxIOPool, 0)->angle - yawZeroAngle) * 360 / 8192.0f;
			NORMALIZE_ANGLE180(yawRealAngle);
			if(GYRO_RESETED == 2) {
/******��������1  yaw�Ƕ�*******/
			yawRealAngle = -ZGyroModuleAngle;
//		fw_printfln("GMYawEncoder.ecd_angle:%f",GMYawEncoder.ecd_angle);
			}
/*����ģʽ�л�*/
			if(GetShootMode() == AUTO) {
				if((GetLocateState() == Locating) && (CReceive != 0))	{
//				yawAngleTarget = yawRealAngle - (yawAdd *0.21f);
				yawAngleTarget = yawRealAngle - yawAdd ;
				fw_printfln("yawAdd:%f",yawAdd );
				CReceive--;
				}
				else if((GetLocateState() == Located) && (rune_flag != 0)){
					if(GetRuneState() == AIMING){
						fw_printfln("rune:%d", rune);
						yawAngleTarget = Location_Number[rune - 1].yaw_position;
						rune_flag--;
					}
				}
		  }
		
						
//			MINMAX(yawAngleTarget, -45, 45);
			yawIntensity = PID_PROCESS_Double(yawPositionPID,yawSpeedPID,yawAngleTarget,yawRealAngle,-gYroZs);

//      fw_printfln("yawIntensity:%d", yawIntensity);
			setMotor(GMYAW, yawIntensity);
		}
/*��̨pitch��*/
		if(IOPool_hasNextRead(GMPITCHRxIOPool, 0)){
			
			uint16_t pitchZeroAngle = 3180;
			int16_t pitchIntensity = 0;
			
			IOPool_getNextRead(GMPITCHRxIOPool, 0);
			pitchRealAngle = -(IOPool_pGetReadData(GMPITCHRxIOPool, 0)->angle - pitchZeroAngle) * 360 / 8192.0;
			NORMALIZE_ANGLE180(pitchRealAngle);
/*******��������2 Pitch�Ƕ�******/
//			fw_printfln("pitchRealAngle:%f",pitchRealAngle);
			if(GetShootMode() == AUTO) {
				if((GetLocateState() == Locating) && (CReceive != 0))	{
//				yawAngleTarget = yawRealAngle - (yawAdd *0.21f);
//					fw_printfln("pitchRealAngle:%f",pitchRealAngle );
					fw_printfln("pitchAdd:%f",pitchAdd );
			pitchAngleTarget = pitchRealAngle + pitchAdd ;
//			if(pitchAdd >= 0){
//				pitchAngleTarget = pitchRealAngle + (0.1f*pitchAdd) ;//0.665
//			}
//			else{
//				pitchAngleTarget = pitchRealAngle + (0.08f*pitchAdd) ;//0.62
//			}
//					pitchAngleTarget = pitchRealAngle + pitchAdd ;
				CReceive --;
				}
				else if((GetLocateState() == Located) && (rune_flag != 0)){
					if(GetRuneState() == AIMING){
						pitchAngleTarget = Location_Number[rune - 1].pitch_position;
						rune_flag--;
					}
				}
		  }
//		if((GetShootMode() == AUTO) && (CReceive != 0))	{
////					fw_printfln("pitchRealAngle:%f",pitchRealAngle );
////					fw_printfln("pitchAdd:%f",pitchAdd );
//			pitchAngleTarget = pitchRealAngle + (0.3f*pitchAdd) ;
////			if(pitchAdd >= 0){
////				pitchAngleTarget = pitchRealAngle + (0.1f*pitchAdd) ;//0.665
////			}
////			else{
////				pitchAngleTarget = pitchRealAngle + (0.08f*pitchAdd) ;//0.62
////			}
////					pitchAngleTarget = pitchRealAngle + pitchAdd ;
//				CReceive --;
//			}
			MINMAX(pitchAngleTarget, -25, 25);
			pitchIntensity = PID_PROCESS_Double(pitchPositionPID,pitchSpeedPID,pitchAngleTarget,pitchRealAngle,-gYroXs);
			//		fw_printfln("pitchIntensity:%d", pitchIntensity);
			setMotor(GMPITCH, pitchIntensity);
		}
//���̵�� 1 2 3 4	

		if(IOPool_hasNextRead(CMFLRxIOPool, 0)){
			IOPool_getNextRead(CMFLRxIOPool, 0);
			Motor820RRxMsg_t *pData = IOPool_pGetReadData(CMFLRxIOPool, 0);
			CANReceiveMsgProcess_820R(pData, &CM2Encoder);
#ifdef MOTOR_ARMED
			CM2SpeedPID.ref =  -ChassisSpeedRef.forward_back_ref*0.075 + ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;
#else 
			CM2SpeedPID.ref =  ChassisSpeedRef.forward_back_ref*0.075 + ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;
#endif
			CM2SpeedPID.fdb = CM2Encoder.filter_rate;
		  CM2SpeedPID.Calc(&CM2SpeedPID);
		  setMotor(CMFR, CHASSIS_SPEED_ATTENUATION * CM2SpeedPID.output);
		}
		  if(IOPool_hasNextRead(CMFRRxIOPool, 0)){
			IOPool_getNextRead(CMFRRxIOPool, 0);
			Motor820RRxMsg_t *pData = IOPool_pGetReadData(CMFRRxIOPool, 0);
			CANReceiveMsgProcess_820R(pData, &CM1Encoder);
#ifdef MOTOR_ARMED		 
		  CM1SpeedPID.ref =  ChassisSpeedRef.forward_back_ref*0.075 + ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;
#else
			CM1SpeedPID.ref =  -ChassisSpeedRef.forward_back_ref*0.075 + ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;	
#endif
			CM1SpeedPID.fdb = CM1Encoder.filter_rate;
//		 fw_printfln("CM1SpeedPID.ref:%f",CM1SpeedPID.ref);
//		 fw_printfln("CM1Encoder.filter_rate:%d",CM1Encoder.filter_rate);
		  CM1SpeedPID.Calc(&CM1SpeedPID);
		  setMotor(CMFL, CHASSIS_SPEED_ATTENUATION * CM1SpeedPID.output);
		}
		if(IOPool_hasNextRead(CMBLRxIOPool, 0)){
			IOPool_getNextRead(CMBLRxIOPool, 0);
			Motor820RRxMsg_t *pData = IOPool_pGetReadData(CMBLRxIOPool, 0);
			CANReceiveMsgProcess_820R(pData, &CM3Encoder);
			
			CM3SpeedPID.ref =  ChassisSpeedRef.forward_back_ref*0.075 - ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;
			CM3SpeedPID.fdb = CM3Encoder.filter_rate;
		  CM3SpeedPID.Calc(&CM3SpeedPID);
		  setMotor(CMBL, CHASSIS_SPEED_ATTENUATION * CM3SpeedPID.output);
		}
		if(IOPool_hasNextRead(CMBRRxIOPool, 0)){
			IOPool_getNextRead(CMBRRxIOPool, 0);
			Motor820RRxMsg_t *pData = IOPool_pGetReadData(CMBRRxIOPool, 0);
			CANReceiveMsgProcess_820R(pData, &CM4Encoder);
			
			CM4SpeedPID.ref =  -ChassisSpeedRef.forward_back_ref*0.075 - ChassisSpeedRef.left_right_ref*0.075 + ChassisSpeedRef.rotate_ref;
		  CM4SpeedPID.fdb = CM4Encoder.filter_rate;
		  CM4SpeedPID.Calc(&CM4SpeedPID);
		  setMotor(CMBR, CHASSIS_SPEED_ATTENUATION * CM4SpeedPID.output);
		}
	}
}
