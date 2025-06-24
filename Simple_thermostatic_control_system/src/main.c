#include "reg52.h"
#include "temp.h"	
#include "LCD1602.h"
#include "XPT2046.h"

typedef unsigned int u16;
typedef unsigned char u8;

sbit PWM = P1^0;  // 电机PWM控制引脚
sbit BUZZER = P1^5;     // 蜂鸣器控制引脚
sbit IRIN = P3^2;         // 红外接收引脚

u8 pwm_enable = 0;  // 电机运行状态
u8 duty = 0;       // PWM占空比
u8 control_mode = 0;   // 控制模式：0-自动，1-红外，2-电位器
u8 display_data[8];
u8 IrValue[6];   // 存储红外接收到的值
u8 Time;         // 时间计数
int temp, real_temp; // 真 ~ 温度

void delay(u16 i)
{
	while(i--);	
}

void Buuzer_Time(u16 delay_time)
{
    u16 x;
    for(x=0; x<delay_time; x++)
    {
        BUZZER = !BUZZER;
        delay(1);   // 1k Hz
    }
    BUZZER = 0; 
}

void datapros(int temp) 	 
{
    int actual_temp;
    if(temp < 0)
    {
        display_data[0] = '-'; 	  
        temp = temp - 1;
        temp = ~temp;
        actual_temp = (temp / 16) * 100;
    }
    else
    {			
        display_data[0] = ' ';
        actual_temp = (temp / 16) * 100;
    }
    
    // 显示整数部分
    display_data[1] = actual_temp / 10000;
    display_data[2] = actual_temp % 10000 / 1000;
    display_data[3] = actual_temp % 1000 / 100;
    display_data[4] = actual_temp % 100 / 10;
    display_data[5] = actual_temp % 10; 
}

void LCD_ShowTemp_State()
{
    LCD_ShowString(1, 1, "Temp:");          // 第一行显示温度

    LCD_ShowChar(1, 6, display_data[0]);
    LCD_ShowChar(1, 7, display_data[1] + '0');  
    LCD_ShowChar(1, 8, display_data[2] + '0');  
    LCD_ShowChar(1, 9, display_data[3] + '0');  

    LCD_ShowChar(1, 10, '.');

    LCD_ShowChar(1, 11, display_data[4] + '0');  
    LCD_ShowChar(1, 12, display_data[5] + '0'); 

    LCD_ShowChar(1, 13, 0xDF);  
    LCD_ShowChar(1, 14, 'C');   

    LCD_ShowString(2, 1, "M:");  // 第二行显示控制模式和电机转速
    switch(control_mode)
    {
        case 0:
            LCD_ShowString(2, 3, "T  A");  // 自动模式
            break;
        case 1:
            LCD_ShowString(2, 3, "I  R");  // 手动模式
            break;
        case 2:
            LCD_ShowString(2, 3, "Pot ");  // 电位器控制模式
            break;
    }

    // 显示电机转速
    LCD_ShowString(2, 8, "RPM:");
    if(!pwm_enable){LCD_ShowString(2, 12, "STOP");}
    else{LCD_ShowNum(2, 12, duty, 4); }
}

// 红外初始化函数
void IrInit()
{
    IT0=1;    // 下降沿触发
    EX0=1;    // 打开中断0允许
    EA=1;     // 打开总中断
    IRIN=1;   // 初始化端口
}

void Timer0Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 0xFC;
    TL0 = 0x66;
    EA = 1;
    ET0 = 1;
    TR0 = 1;
}

void ad_read()        // 读取电位器函数
{
    u16 pot_value = Read_AD_Data(0x94);
    if(pot_value < 100)  // 如果电位器值太小，停止电机
    {
        pwm_enable = 0;
        duty = 0;
        PWM = 0;
    }
    else
    {
        pwm_enable = 1;
        // 扩大电位器控制范围，将100-4095映射到1-100
        if(pot_value < 1000)  // 低段
        {
            duty = 1 + (pot_value - 100) * 24 / (1000 - 100);  // 映射到1-25
        }
        else if(pot_value < 2000)  // 中段
        {
            duty = 25 + (pot_value - 1000) * 25 / (2000 - 1000);  // 映射到25-50
        }
        else  // 高段
        {
            duty = 50 + (pot_value - 2000) * 25 / (4095 - 2000);  // 映射到50-100
        }
        PWM = 1;
    }
}

void Motor(int temp)
{
    if(control_mode == 2)  // 电位器控制
    {
        ad_read();
        return;
    }

    else if(control_mode == 0)  // 自动模式
    {
        int temp_diff = temp - (26 * 100);  // 温差

        if(temp_diff > 100)  // 温差大于4度
        {
            pwm_enable = 1;
            duty = 192;
            PWM = 1;     
            Buuzer_Time(5000);  // 蜂鸣器报警
        }
        else if(temp_diff > 50)  // 温差2-4度
        {
            pwm_enable = 1;
            duty = 128;
            PWM = 1;     
            Buuzer_Time(5000);  // 蜂鸣器报警
        }
        else if(temp_diff > 0)  // 温差大于0度
        {
            pwm_enable = 1;
            duty = 64;   // 低速运行
            PWM = 1;     
            Buuzer_Time(5000);  // 蜂鸣器报警
        }
        else  
        {
            pwm_enable = 0;  // 停止电机
            duty = 0;        // 设置占空比为0
            PWM = 0;         // 关闭PWM输出
            BUZZER = 0;      // 关闭蜂鸣器
        }
    }
}

void Timer0(void) interrupt 1
{
    u8 pwm_count = 0;
    TL0 = 0x66;
    TH0 = 0xFC;
    
    pwm_count++;
    if(pwm_count >= 256)
    {
        pwm_count = 0;
    }
    
    if(pwm_enable)  
    {
        if(pwm_count < duty)
        {
            PWM = 1;  
        }
        else
        {
            PWM = 0;  
        }
    }
    
    else
    {
        PWM = 0; 
    }
}

// 处理红外按键函数
void Ir_Proces()
{
    if(IrValue[2] == 0x45)  // 电源键
    {
        pwm_enable = !pwm_enable;  // 切换电机开关
        if(!pwm_enable)
        {
            duty = 0;
        }
        Buuzer_Time(1000);  // 按键提示音
    }

    else if(IrValue[2] == 0x16)  // 使用0键切换控制模式
    {
        control_mode = (control_mode + 1) % 3;  // 循环切换三种模式
        Buuzer_Time(500);
    }

    else if(control_mode == 1 && pwm_enable)  // 手动模式下处理速度调节
    {
        switch(IrValue[2])
        {
            case 0x0C:  // 1键低速
                duty = 64;
                break;
            case 0x18:  // 2键中速
                duty = 128;
                break;
            case 0x5E:  // 3键高速
                duty = 192;
                break;
        }
        Buuzer_Time(500);  // 按键提示音
    }
}

void ReadIr() interrupt 0
{
    u8 j,k;
    u16 err;
    Time=0;          
    delay(700); //7ms
    if(IRIN==0)   //确认是否收到正确的信号
    {  
        err=1000;       //1000*10us=10ms,超过说明接收到错误的信号
        while((IRIN==0)&&(err>0)) //等待前面9ms的低电平过去     
        {     
            delay(1);
            err--;
        } 
        if(IRIN==1)     //如果正确等到9ms低电平
        {
            err=500;
            while((IRIN==1)&&(err>0))    //等待4.5ms的起始高电平过去
            {
                delay(1);
                err--;
            }
            for(k=0;k<4;k++)    //共有4组数据
            {       
                for(j=0;j<8;j++)  //接收一组数据
                {
                    err=60;   
                    while((IRIN==0)&&(err>0))//等待信号前面的560us低电平过去
                    {
                        delay(1);
                        err--;
                    }
                    err=500;
                    while((IRIN==1)&&(err>0))  //计算高电平的时间长度。
                    {
                        delay(10);   //0.1ms
                        Time++;
                        err--;
                        if(Time>30)
                        {
                            return;
                        }
                    }
                    IrValue[k]>>=1;  //k表示第几组数据
                    if(Time>=8)         //如果高电平出现大于565us，那么是1
                    {
                        IrValue[k]|=0x80;
                    }
                    Time=0;     //用完时间要重新赋值                            
                }
            }
        }
        if(IrValue[2]!=~IrValue[3])
        {
            return;
        }
        Ir_Proces();  // 处理接收到的红外按键
    }           
}

void main()
{	
	LCD_Init();
	IrInit();  // 初始化红外接收
	Timer0Init();  // 初始化定时器
	
	while(1)
	{
		EA=0;
        real_temp = Ds18b20ReadTemp();
		datapros(real_temp);
		EA=1;
		LCD_ShowTemp_State();
		temp = (real_temp / 16 )* 100;  // 0.0625 = 1/16
		// 确保温度值正确
		if(temp < 0) 
        {
			temp = 0;
		}
		Motor(temp);
		delay(1000);
	}		
}

