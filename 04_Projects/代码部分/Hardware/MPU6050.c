#include "myfile.h"

float pitch,roll,yaw;
int16_t AX, AY, AZ, GX, GY, GZ;

#define MPU6050_ADDRESS		0xD0
float dt = 0.07f;
void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data)
{
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(Data);
	MyI2C_ReceiveAck();
	MyI2C_Stop();
}

uint8_t MPU6050_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS);
	MyI2C_ReceiveAck();
	MyI2C_SendByte(RegAddress);
	MyI2C_ReceiveAck();
	
	MyI2C_Start();
	MyI2C_SendByte(MPU6050_ADDRESS | 0x01);
	MyI2C_ReceiveAck();
	Data = MyI2C_ReceiveByte();
	MyI2C_SendAck(1);
	MyI2C_Stop();
	
	return Data;
}

void MPU6050_Init(void)
{
	MyI2C_Init();
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);
	MPU6050_WriteReg(MPU6050_CONFIG, 0x06);
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x18);
}

uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

void MPU6050_GetData(int16_t *AccX, int16_t *AccY, int16_t *AccZ, 
						int16_t *GyroX, int16_t *GyroY, int16_t *GyroZ )

{
	// 从 MPU6050 获取原始加速度和角速度数据
uint8_t DataH, DataL;

// 获取加速度值
DataH = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H);
DataL = MPU6050_ReadReg(MPU6050_ACCEL_XOUT_L);
*AccX = (DataH << 8) | DataL;


DataH = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_H);
DataL = MPU6050_ReadReg(MPU6050_ACCEL_YOUT_L);
*AccY = (DataH << 8) | DataL;


DataH = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_H);
DataL = MPU6050_ReadReg(MPU6050_ACCEL_ZOUT_L);
*AccZ = (DataH << 8) | DataL;

// 获取角速度值
DataH = MPU6050_ReadReg(MPU6050_GYRO_XOUT_H);
DataL = MPU6050_ReadReg(MPU6050_GYRO_XOUT_L);
*GyroX = (DataH << 8) | DataL;


DataH = MPU6050_ReadReg(MPU6050_GYRO_YOUT_H);
DataL = MPU6050_ReadReg(MPU6050_GYRO_YOUT_L);
*GyroY = (DataH << 8) | DataL;


DataH = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_H);
DataL = MPU6050_ReadReg(MPU6050_GYRO_ZOUT_L);
int16_t RawGyroZ = ((DataH << 8) | DataL);
*GyroZ = RawGyroZ /16.4;				//实际角速度
yaw += (float)(*GyroZ) * dt;   	//实际偏航角


// roll = my_atan2((float)(*AccX), my_sqrt((float)((*AccY) * (*AccY )+ (*AccZ) * (*AccZ)))) * 180.0f / 3.14159f;  // 俯仰角（单位：弧度）
// pitch = my_atan2((float)(*AccY), my_sqrt((float)((*AccX) * (*AccX) + (*AccZ) * (*AccZ)))) * 180.0f / 3.14159f;  // 滚转角（单位：弧度）
// float acc_pitch = my_atan2((float)(*AccY), my_sqrt((float)((*AccX) * (*AccX) + (*AccZ) * (*AccZ)))); 
// acc_pitch = acc_pitch * 180.0f / 3.14159f;  // 转换为度
// Kalman_Cal_Pitch(acc_pitch, *GyroY); 

}

void Clear_yaw(void) 
{
    yaw = 0.0f;  // 直接重置为0
}


