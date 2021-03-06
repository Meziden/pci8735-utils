#include <stdio.h>
#include <stdlib.h>
#include "stdafx.h"
#include "windows.h"

// 驱动接口头文件(必须)
#include "PCI8735.h"

//需要启动的设备的硬件编号，如只有一张采集卡请将该宏定义为0
#define DEVICE_HW_SERIAL 0

//程序初始输出
void greetings()								
{
	puts("PCI8735数据采集卡实用程序（AD模块）");
	puts("");
	puts("该文件夹下已经存在的采集数据将会被删除，请按Enter继续");
}

//数字IO通道控制

//通道对齐时所需的数据处理公式
double functions(double &cache,int channel)
{
	switch(channel)
	{
		//速度/rpm
		case 0:return cache;
		break;
		//电压/V
		case 1:return cache;
		break;
		//电流/A
		case 2:return cache;
		break;
		//转子分压/V
		case 3:return cache;
		break;
		//转子电流/A
		case 4:return cache;
		break;
		case 5:return cache;
		break;
		case 6:return cache;
		break;
		case 7:return cache;
		break;
		case 8:return cache;
		break;
		case 9:return cache;
		break;
		case 10:return cache;
		break;
		case 11:return cache;
		break;
		case 12:return cache;
		break;
		case 13:return cache;
		break;
		case 14:return cache;
		break;
		case 15:return cache;
		break;
	}
	//报告错误
	puts("Error");
	return 0;
}

int main()
{
	//程序初始输出
	greetings();
	
	//确认操作
	while(getchar()!='\n')
	{continue;}
	
	//RS485一次性读取的数据量
	DWORD num=16;
	DWORD num_return;
	int speed_rpm[12]={20,40,60,80,100,120,140,160,180,200,220,250};

	//初始化RS485
	HANDLE rs485port=CreateFile("COM3",GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	
	//检查端口是否正常启用
	if(rs485port==(HANDLE)-1)
	{
		puts("端口初始化失败");
		Sleep(700);
		exit(1);
	}
	
	//RS485端口参数结构体
	DCB dcb;
	memset( &dcb, 0, sizeof(dcb) );
	
	//RS485端口参数结构体：
	dcb.DCBlength = sizeof(DCB);
	dcb.fBinary = TRUE;       // binary mode, no EOF check
	dcb.fParity = FALSE;       // enable/disable parity checking
	dcb.fOutxCtsFlow = FALSE;     // CTS output flow control
	dcb.fOutxDsrFlow = FALSE;     // DSR output flow control
	dcb.fDtrControl = DTR_CONTROL_DISABLE;     // DTR flow control type DTR_CONTROL_HANDSHAKE;
	dcb.fDsrSensitivity = FALSE;   // DSR sensitivity
	dcb.fTXContinueOnXoff = FALSE; // XOFF continues Tx
	dcb.fOutX = FALSE;       // XON/XOFF out flow control
	dcb.fInX = FALSE;       // XON/XOFF in flow control
	dcb.fErrorChar = FALSE;   // enable error replacement
	dcb.fNull = FALSE;       // enable null stripping
	dcb.fAbortOnError = FALSE;   // abort on error
	dcb.BaudRate = 9600;         //波特率使用硬件厂商推荐数
	dcb.ByteSize = 8;            // ASCII
	dcb.Parity = 0;     // 0-4=no,odd,even,mark,space
	dcb.StopBits = 2;   // 0,1,2 = 1, 1.5, 2
	dcb.fRtsControl = RTS_CONTROL_TOGGLE;//参考MSDN
	
	//检测端口设置情况
	if(SetCommState( rs485port, &dcb )==0)
	{
		puts("RS485端口设置失败。");
		Sleep(700);
		exit(2);
	}
	
	//准备RS485端口
	PurgeComm(rs485port,PURGE_TXCLEAR | PURGE_RXCLEAR);
	SetupComm(rs485port,1024,1024);
	
	//数据存储文件路径
	FILE *target=fopen("./testdata_raw.tdf","w");
	
	//检测目标文件是否正常打开
	if(target==NULL)
	{
		printf("-----错误警告-----\n原始数据存储文件打开/创建失败，请检查磁盘工作情况或检查文件是否被其他程序占用。\n");
	}
	//配置文件路径
	FILE *config=fopen("./AD_Convert.cfg","r");
	
	//检测目标文件是否正常打开
	if(config==NULL)
	{
		printf("\n-----错误警告-----\n配置文件无法打开，请检查配置文件是否被其他程序占用。\n");
	}
	
	//返回实际读取量
	LONG nRetSizeWords;
	
	//创建设备对象:adport
	HANDLE adport=PCI8735_CreateDevice(DEVICE_HW_SERIAL);
	
	//检验设备是否初始化成功
	if(adport==INVALID_HANDLE_VALUE)
	{
		printf("\n-----错误警告-----\n模数转换设备对象创建失败，请检查PCI设备连接情况。\n");
	}
	
	//AD初始化所需结构体，定义位于PCI8735.h
	int SampleNum;
	PCI8735_PARA_AD ADPara;
	memset(&ADPara, 0, sizeof(ADPara));
	fscanf(config,"%d",&SampleNum);
	fscanf(config,"%d",&ADPara.FirstChannel);
	fscanf(config,"%d",&ADPara.LastChannel);
	fscanf(config,"%d",&ADPara.InputRange);
	fscanf(config,"%d",&ADPara.GroundingMode);
	fscanf(config,"%d",&ADPara.Gains);
	
	//输出设备参数
	printf("AD转换器当前设备参数为 %d %d %d %d %d \n",ADPara.FirstChannel,ADPara.LastChannel,ADPara.InputRange,ADPara.GroundingMode,ADPara.Gains);
	printf("目标数据合计%d组\n",SampleNum);
	//自动获取活动的通道数量
	int ChannelCount=ADPara.LastChannel-ADPara.FirstChannel+1;
	
	//建立一个缓冲区
	USHORT ADBuffer[8192];
	
	//用于舍弃数据块末尾无法对齐的数据
	int nReadSizeWords = 512 - 512 % ChannelCount;
	
	//初始化AD设备
	PCI8735_InitDeviceAD(adport,&ADPara);
	
	//等待设备初始化完成，防止初始化干扰
	Sleep(2000);
	
	puts("");
	puts("数据采集开始");
	
	//进度显示
	printf("已采集   0组数据");
	
	int rpmswitch=0;
	for(int j=0;j<SampleNum;j++)
	{	
		//转速调节
		if(j%200==0)
			{
				switch(rpmswitch)
				{
					//重要：
					//需要传输的信号在此设定（0106 0042(66(dec)) TARGET_RPM_HEX CHK CHK \CR \LF）
					//此处的字符串并非对应使用时所需字符串，请查阅电机驱动器获得详细信息
					case 0:WriteFile(rs485port,":010600420000B7\r\n",num,&num_return,NULL);break; //0
					case 1:WriteFile(rs485port,":01060042001E99\r\n",num,&num_return,NULL);break; //200
					case 2:WriteFile(rs485port,":01060042003C7B\r\n",num,&num_return,NULL);break; //400
					case 3:WriteFile(rs485port,":01060042005A5D\r\n",num,&num_return,NULL);break; //600
					case 4:WriteFile(rs485port,":0106004200783F\r\n",num,&num_return,NULL);break; //800
					case 5:WriteFile(rs485port,":01060042009621\r\n",num,&num_return,NULL);break; //1000
					case 6:WriteFile(rs485port,":0106004200B403\r\n",num,&num_return,NULL);break; //1200
					case 7:WriteFile(rs485port,":0106004200D2E5\r\n",num,&num_return,NULL);break; //1400
					case 9:WriteFile(rs485port,":0106004200F0C7\r\n",num,&num_return,NULL);break; //1600
					case 10:WriteFile(rs485port,":01060042010EA8\r\n",num,&num_return,NULL);break; //1800
					case 11:WriteFile(rs485port,":01060042012C8A\r\n",num,&num_return,NULL);break; //2000
					case 12:WriteFile(rs485port,":01060042012C8A\r\n",num,&num_return,NULL);break; //2200
				}
				printf("\r采集%4d组数据,目标频率%3dHz",j,speed_rpm[rpmswitch]);
				rpmswitch++;
			}
		//读取数据，存入内存缓冲ADBuffer
		PCI8735_ReadDeviceAD(adport,ADBuffer,nReadSizeWords,&nRetSizeWords);
		
		//将内存缓冲ADBuffer的数据同步到硬盘中
		for(int i=0;i<nReadSizeWords;i++)
		{
			fprintf(target,"%d\n",ADBuffer[i]);
		}
	}
	
	//停止,释放AD对象
	PCI8735_ReleaseDeviceAD(adport);
	
	//释放设备对象
	PCI8735_ReleaseDevice(adport);
	
	//关闭文件
	fclose(target);
	 
	//开始数据处理
	puts("");
	puts("数据采集完成，正在执行数据通道对齐转换。");
	
	//通道对齐转换初始化
	double cache;
	int counter=1,row,total=1;
	FILE *datain,*dataout;
	puts("");
	row=ChannelCount;
	datain=fopen("testdata_raw.tdf","r");
	
	//计算需要处理的数据量
	char c;
	while((c=getc(datain))!=EOF)
	{
		if(c=='\n')
		{
			total++;
		}
	}
	
	fclose(datain);
	datain=fopen("testdata_raw.tdf","r");
	dataout=fopen("testdata_converted.tdf","w");
	printf("数据转换执行中...\n  0.0%%");
	
	//输出到文件与进度显示
	while(fscanf(datain,"%lf",&cache)!=EOF)
	{

		fprintf(dataout,"%10.2lf\t",functions(cache,ChannelCount-row));
		row--;

		if(row==0)
		{	
			counter++;
			if(counter%500==0)
			{
				printf("\r%5.1f%%",100.0*counter/(total/ChannelCount));
			}
			fprintf(dataout,"\n");
			row=ChannelCount;
		}
	}
	printf("\b\b\b\b\b\b 100.0%%");
	
	//关闭文件
	fclose(datain);
	fclose(dataout);
	
	//启动数据分析程序
	system("start ./AD_DATA_ANA");
	
	//程序结束
	printf("\nAD数据采集程序结束，将在5秒后退出。");
	Sleep(5000);
	return 0;
}
