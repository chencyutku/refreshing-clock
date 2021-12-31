/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "ht32_board.h"
#include "ht32_board_config.h"
#include "tlpalarm.h"
#include "LCD_Screen.h"
#include <stdlib.h>

/* Macro ---------------------------------------------------------------------------------------------------*/

// Reference to tlpalarm.h



/* Private variables ---------------------------------------------------------------------------------------*/

// MCTM 控  LED色溫
TM_TimeBaseInitTypeDef MCTM_TimeBaseInitStructure;
TM_OutputInitTypeDef MCTM_OutputInitStructure;
MCTM_CHBRKCTRInitTypeDef MCTM_CHBRKCTRInitStructure;

// GPTM 控 鬧鐘鈴聲
TM_TimeBaseInitTypeDef   GPTM_TimeBaseInitStructure;
TM_OutputInitTypeDef     GPTM_OutputInitStructure;
__IO uint32_t wMctmUpdateDownCounter = 0;

// 上拉電阻按鈕陣列
FlagStatus button[12];
int cur_button = -1;
// PA0~PA3為掃描線
// PB0~PA3為資料線

int temper_L2H_flag = 0;
int temper_H2L_flag = 0;

// 現在時間
int abs_sec = PRESET_ABS_SEC;  // Absolute second
unsigned int sec_after_boot = 0;        // 用來給srand()種子碼用的
int hr_t = 0;
int min_t = 0;

int setting_sec = 0;
int time_set_cursor = 0;
int time_set_hr_ten = 0;

int display_hr;
int display_min;
int display_sec;
int real_sec;
int real_min;
int real_hr;

int alarm_time_s = PRESET_ALARM_SEC;

int day_night_flag;
int target_time_s; // 預計LED色溫變化終點的時間

// 目前的時鐘模式，[ 顯示目前時間 | 設定時間 | 設定鬧鐘 | 顯示計算式 ]
int clock_mode = CUR_TIME;
int correct_password = 0;

/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************/ /**
  * @brief  Main program.
  * @retval None
  ***********************************************************************************************************/


/* 初始化設定 ----------------------------------------------------------------------------------------------*/
void CKCU_Configuration(void);
void GPIO_Configuration(void);
//void EXTI_Configuration(void);
void NVIC_Configuration(void);
void BFTM_Configuration(void);
void MCTM_Configuration(void);

/* 調整性設定 ----------------------------------------------------------------------------------------------*/
void Delay(u32 cnt) { while(cnt--); }

// LED
void R_LED(int level);
void G_LED(int level);
void B_LED(int level);
void LED_Set_Temperature(int Temperature);
void LED_temper_L2H(void);

// 時鐘
void DisplayLCD(void);

// 鬧鐘設定
void GPTM_Configuration(void);                      // 蜂鳴器操作
void GPTM_Time_Configuration(float Frequency);      // 蜂鳴器操作
void GPTM_Time_Disable(int delay);                  // 蜂鳴器操作
int TimeBeforeTarget(int target_s);                 // 鈴響之前剩餘時間
void Little_Star(void);


// LCD Screen
u8 _backlightval;
u8 _displaycontrol;
u8 _displayfunction;
void Delay_us(int cnt);
void I2C_Write(HT_I2C_TypeDef* I2Cx, u16 slave_address, u8* buffer, u8 BufferSize);
void I2C_Read(HT_I2C_TypeDef* I2Cx, u16 slave_address, u8* buffer, u8 BufferSize);
void LCD_I2C_1602_4bit_Write(u8 data);
void LCD_command(u8 command);
void LCD_ini(void);
void LCD_Backlight(u8 enable);
void I2C_Configuration(void);
void LCD_setCursor(uint8_t col, uint8_t row);
void LCD_Write(u8 Data);

void ClearLCD(int row);
void TestLCD(void);

int GenRandomMathExpression(void);
void DisplayMathExpression(int a, int b);
void DisplayMathInput(int product);

// Button Array
void Read_Button(void);
int Get_Current_Button(void);

int main(void)
{
	int TBT;
    Delay_us(10000);

    CKCU_Configuration(); /* System Related configuration                                       */
    GPIO_Configuration(); /* GPIO Related configuration                                         */
    BFTM_Configuration();
    NVIC_Configuration();
    MCTM_Configuration();
	GPTM_Configuration();
	
	// for LCD Screen
	I2C_Configuration();
    RETARGET_Configuration();
	LCD_ini();

    printf("Test LCD_ini\r\n");


    while (1)
    {
        // 日夜狀態標記切換
        if(abs_sec >= MORNING && abs_sec < EVENING)
            day_night_flag = DAY;
        else
            day_night_flag = NIGHT;
        
        // 決定target_time要設為 [早上 | 鬧鐘 | 傍晚]
        if(day_night_flag == NIGHT)
        {
            if(alarm_time_s == NO_ALARM)
            {
                target_time_s = MORNING;
            }
            else
            {
                target_time_s = alarm_time_s;
            }
        }
        else
        {
            target_time_s = EVENING;
        }

        // 檢測並觸發LED色溫變化
        TBT = TimeBeforeTarget(target_time_s);
        // printf("%s\r\n", (day_night_flag) ? "DAY":"NIGHT");
        // printf("Before Target sec: %d\r\n", TBT);
        if(TBT <= (TEMP_PREACT_TIME))
        {
            if(alarm_time_s == target_time_s && temper_L2H_flag == 0)
            {
                temper_L2H_flag = 1;
                temper_H2L_flag = 0;
            }
            else if(day_night_flag == DAY && temper_L2H_flag == 0)
            {
                temper_L2H_flag = 1;
                temper_H2L_flag = 0;
            }
            else if(day_night_flag == NIGHT && temper_H2L_flag == 0)
            {
                temper_H2L_flag = 1;
                temper_L2H_flag = 0;
            }
        }
        else
        {
            temper_L2H_flag = 0;
            temper_H2L_flag = 0;
        }

        if ((target_time_s <= abs_sec) &&  (target_time_s <= alarm_time_s + 1))
        {// 抵達目標時間
            temper_H2L_flag = 0;
            temper_L2H_flag = 0;
            if(target_time_s == alarm_time_s)
            {// 目標時間就是鬧鐘時間
                clock_mode = MATH_EXPRESSION;
			    Little_Star(); // 要注意，執行這邊時整個程式會卡在這裡，其他東西要執行必須使用 Interrupt
            }
        }

        // TestLCD();
    }
}

/*********************************************************************************************************//**
  * @brief  Initial Setting
  * @retval None
  ***********************************************************************************************************/
void CKCU_Configuration(void)
{
    CKCU_PeripClockConfig_TypeDef CKCUClock = {{0}};
    
    
    CKCUClock.Bit.AFIO  = 1;

	CKCUClock.Bit.PA    = 1;
    CKCUClock.Bit.PB    = 1; // 必要，別刪
	CKCUClock.Bit.PC    = 1;
	CKCUClock.Bit.PD    = 1;
	
	CKCUClock.Bit.I2C0  = 1;
	CKCUClock.Bit.I2C1  = 1;

    CKCUClock.Bit.BFTM0 = 1;
    CKCUClock.Bit.BFTM1 = 1;
    CKCUClock.Bit.GPTM0 = 1;
    CKCUClock.Bit.MCTM0 = 1;
	
	//對設立旗標的功能之時脈閘進行更動
    CKCU_PeripClockConfig(CKCUClock, ENABLE);
}
void BFTM_Configuration(void)
{
    //將BFTM設為0.5秒為一週期: 1/0.5S = 2Hz <=> 1/2Hz = 0.5s
    //SystemCoreClock 為SDK內建參數，AHB時鐘跑1秒的計數量
    //float Time_s = 1;
    float Frequency_Hz = 1;
    float Button_Scan_Freq_Hz = 4;
    
    //Compare值擇一宣告
    //int Compare = SystemCoreClock * Time_s;
    int Compare = SystemCoreClock / Frequency_Hz;
    int Compare_Button_Scan = SystemCoreClock / Button_Scan_Freq_Hz;

    BFTM_SetCompare(HT_BFTM0,Compare);
    //計時器歸0
    BFTM_SetCounter(HT_BFTM0, 0);
    //不啟用單次模式
    BFTM_OneShotModeCmd(HT_BFTM0, DISABLE);
    //BTFM中斷源開啟
    BFTM_IntConfig(HT_BFTM0, ENABLE);
    //BFTM開啟
    BFTM_EnaCmd(HT_BFTM0, ENABLE);
  
  
    /***BFTM1用更快的間隔來真測按鈕***/
    //將BFTM設為0.5秒為一週期: 1/0.5S = 2Hz <=> 1/2Hz = 0.5s
    //SystemCoreClock 為SDK內建參數，AHB時鐘跑1秒的計數量
    //float Time_s = 1;
    
    //Compare值擇一宣告
    //int Compare = SystemCoreClock * Time_s;

    BFTM_SetCompare(HT_BFTM1,Compare_Button_Scan);
    //計時器歸0
    BFTM_SetCounter(HT_BFTM1, 0);
    //不啟用單次模式
    BFTM_OneShotModeCmd(HT_BFTM1, DISABLE);
    //BTFM中斷源開啟
    BFTM_IntConfig(HT_BFTM1, ENABLE);
    //BFTM開啟
    BFTM_EnaCmd(HT_BFTM1, ENABLE);
  
}
void GPIO_Configuration(void)
{
    // 三個MCTM用來控制LED的色溫
    /* Red   LED | C1腳位 MCTM Channel 0 output pin  */
    AFIO_GPxConfig(GPIO_PC, AFIO_PIN_1, AFIO_FUN_MCTM_GPTM);
    /* Green LED | B0腳位  MCTM Channel 1 output pin  */
    AFIO_GPxConfig(GPIO_PB, AFIO_PIN_0, AFIO_FUN_MCTM_GPTM);
    /* Blue  LED | C14腳位  MCTM Channel 3 output pin  */
    AFIO_GPxConfig(GPIO_PC, AFIO_PIN_14, AFIO_FUN_MCTM_GPTM);

    // GPTM0 控制蜂鳴器頻率
    /* Freq set by GPTM0 Channel 3 output pin                                                                    */
    AFIO_GPxConfig(GPIO_PC, AFIO_PIN_9, AFIO_FUN_MCTM_GPTM);

    /* Configure MCTM Break pin */
    AFIO_GPxConfig(GPIO_PB, AFIO_PIN_4, AFIO_FUN_MCTM_GPTM);
	
	
	// For LCD Screen PIN should change by myself
	// PIN再自己改
    AFIO_GPxConfig(GPIO_PB, AFIO_PIN_7 | 
                            AFIO_PIN_8 , AFIO_FUN_I2C);
							// I2C1_SCL
							// I2C1_SDA


    // 按鈕陣列
    //將PA0、PA1、PA2、PA3設定成output
    GPIO_DirectionConfig(HT_GPIOA,  GPIO_PIN_0 | 
                                    GPIO_PIN_1 | 
                                    GPIO_PIN_2 | 
                                    GPIO_PIN_3, GPIO_DIR_OUT);
    // 發現A4腳位有問題，改用A7
    // 將PA7、PA5、PA6設定成input
    GPIO_DirectionConfig(HT_GPIOA,  GPIO_PIN_7 | 
                                    GPIO_PIN_5 | 
                                    GPIO_PIN_6, GPIO_DIR_IN);

    // 為PA7、PA5、PA6 Input 開啟內建上拉電阻
    GPIO_PullResistorConfig(HT_GPIOA,   GPIO_PIN_7 | 
                                        GPIO_PIN_5 | 
                                        GPIO_PIN_6, GPIO_PR_UP);
    // 為PA0、PA1、PA2、PA3關閉內建上拉/下拉電阻
    GPIO_PullResistorConfig(HT_GPIOA,   GPIO_PIN_0 | 
                                        GPIO_PIN_1 | 
                                        GPIO_PIN_2 | 
                                        GPIO_PIN_3, GPIO_PR_DISABLE);

    // 為PA7、PA5、PA6開啟input功能
    GPIO_InputConfig(HT_GPIOA,  GPIO_PIN_7 | 
                                GPIO_PIN_5 | 
                                GPIO_PIN_6, ENABLE);
}
void MCTM_Configuration(void)
{
    /* MCTM Time Base configuration                                                                            */
    MCTM_TimeBaseInitStructure.CounterReload = HTCFG_MCTM_RELOAD - 1;
    MCTM_TimeBaseInitStructure.Prescaler = 0;
    MCTM_TimeBaseInitStructure.RepetitionCounter = 0;
    MCTM_TimeBaseInitStructure.CounterMode = TM_CNT_MODE_UP;
    MCTM_TimeBaseInitStructure.PSCReloadTime = TM_PSC_RLD_IMMEDIATE;
    TM_TimeBaseInit(HT_MCTM0, &MCTM_TimeBaseInitStructure);

    /* MCTM Channel 3 output configuration                                                                    */
    MCTM_OutputInitStructure.Channel = TM_CH_3;
    MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1;
    MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.ControlN = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.IdleState = MCTM_OIS_HIGH;
    MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_LOW;
	
    MCTM_OutputInitStructure.Channel = TM_CH_3;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * 1/2;
    //TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);


    /* MCTM Channel 1 output configuration                                                                    */
    MCTM_OutputInitStructure.Channel = TM_CH_1;
    MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1;
    MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.ControlN = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.IdleState = MCTM_OIS_HIGH;
    MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_LOW;
	
    MCTM_OutputInitStructure.Channel = TM_CH_1;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * 1/2;
    //TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);


    /* MCTM Channel 0 output configuration                                                                    */
    MCTM_OutputInitStructure.Channel = TM_CH_0;
    MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1;
    MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.ControlN = TM_CHCTL_ENABLE;
    MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED;
    MCTM_OutputInitStructure.IdleState = MCTM_OIS_HIGH;
    MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_LOW;
	
    MCTM_OutputInitStructure.Channel = TM_CH_0;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * 1/2;
    //TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);


    /* MCTM Off State, lock, Break, Automatic Output enable, dead time configuration                          */
    MCTM_CHBRKCTRInitStructure.OSSRState = MCTM_OSSR_STATE_ENABLE;
    MCTM_CHBRKCTRInitStructure.OSSIState = MCTM_OSSI_STATE_ENABLE;
    MCTM_CHBRKCTRInitStructure.LockLevel = MCTM_LOCK_LEVEL_2;
    MCTM_CHBRKCTRInitStructure.Break0 = MCTM_BREAK_ENABLE;
    MCTM_CHBRKCTRInitStructure.Break0Polarity = MCTM_BREAK_POLARITY_LOW;
    MCTM_CHBRKCTRInitStructure.AutomaticOutput = MCTM_CHAOE_ENABLE;
    MCTM_CHBRKCTRInitStructure.DeadTime = HTCFG_MCTM_DEAD_TIME;
    MCTM_CHBRKCTRInitStructure.BreakFilter = 0;
    MCTM_CHBRKCTRConfig(HT_MCTM0, &MCTM_CHBRKCTRInitStructure);

    /* MCTM counter enable                                                                                    */
    TM_Cmd(HT_MCTM0, ENABLE);

    /* MCTM Channel Main Output enable                                                                        */
    MCTM_CHMOECmd(HT_MCTM0, ENABLE);
}
// void EXTI_Configuration(void)
// {
//     EXTI_InitTypeDef EXTI_InitStruct;
//     AFIO_EXTISourceConfig(AFIO_EXTI_CH_12, AFIO_ESS_PB);
//     EXTI_InitStruct.EXTI_Channel=EXTI_CHANNEL_12;
//     EXTI_InitStruct.EXTI_Debounce=EXTI_DEBOUNCE_ENABLE;
//     EXTI_InitStruct.EXTI_DebounceCnt=32000;
//     //for 1ms, 1ms*32MHz=32000
//     EXTI_InitStruct.EXTI_IntType=EXTI_POSITIVE_EDGE;
//     EXTI_Init(&EXTI_InitStruct);
//     EXTI_IntConfig(EXTI_CHANNEL_12, ENABLE);
// }
void NVIC_Configuration(void)
{
    NVIC_EnableIRQ(BFTM0_IRQn);
    NVIC_EnableIRQ(BFTM1_IRQn);

    NVIC_SetPriority(BFTM0_IRQn, 0);
    NVIC_SetPriority(BFTM0_IRQn, 1);
    // NVIC_SetPriority(, 1);

    // NVIC_EnableIRQ(EXTI12_IRQn);
    // NVIC_SetPriority(EXTI4_15_IRQn, 0);
}

/*********************************************************************************************************//**
  * @brief  Tweak LED
  * @retval None
  ***********************************************************************************************************/
void R_LED(int level)
{
    if(level > MAX_BRIGHTNESS_LEVEL)
        level = MAX_BRIGHTNESS_LEVEL;
    if(level < 0)
        level = 0;
    MCTM_OutputInitStructure.Channel = TM_CH_0;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * (MAX_BRIGHTNESS_LEVEL - level) / MAX_BRIGHTNESS_LEVEL;
    TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);
}
void G_LED(int level)
{
    if(level > MAX_BRIGHTNESS_LEVEL)
        level = MAX_BRIGHTNESS_LEVEL;
    if(level < 0)
        level = 0;
    MCTM_OutputInitStructure.Channel = TM_CH_1;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * (MAX_BRIGHTNESS_LEVEL - level) / MAX_BRIGHTNESS_LEVEL;
    TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);
}
void B_LED(int level)
{
    if(level > MAX_BRIGHTNESS_LEVEL)
        level = MAX_BRIGHTNESS_LEVEL;
    if(level < 0)
        level = 0;
    MCTM_OutputInitStructure.Channel = TM_CH_3;
    MCTM_OutputInitStructure.Compare = HTCFG_MCTM_RELOAD * (MAX_BRIGHTNESS_LEVEL - level) / MAX_BRIGHTNESS_LEVEL;
    TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);
}
void LED_temper_L2H()
{
    int i, cnt;
    for(i = 0; i < MAX_BRIGHTNESS_LEVEL*2; i++)
    {
        R_LED(i);
        G_LED(i*1.5 - MAX_BRIGHTNESS_LEVEL);
        B_LED(i*1.5 - MAX_BRIGHTNESS_LEVEL);
        for(cnt = 200000; cnt > 0; cnt--);
    }
}
void LED_Set_Temperature(int Temperature)
{
    R_LED(Temperature*1.1 + MAX_BRIGHTNESS_LEVEL/4);
    G_LED(Temperature - MAX_BRIGHTNESS_LEVEL/4);
    B_LED(Temperature - MAX_BRIGHTNESS_LEVEL/4);
}

void DisplayLCD(void)
{
	char current_time_str[10] = {'\0'};
	int str_i;
	
    real_sec = (abs_sec % 60) % 60;
    real_min = (abs_sec / 60) % 60;
    real_hr  = (abs_sec / 60) / 60;
	if(clock_mode != MATH_EXPRESSION)
    {
        display_sec = real_sec;
        display_min = real_min;
        display_hr  = real_hr;
	    sprintf(current_time_str, "%02d:%02d:%02d", display_hr,display_min,display_sec);
	    LCD_setCursor(0,0);//設定游標座標
	    for(str_i = 0; str_i < 8; str_i++)
	    {
	    	LCD_Write(current_time_str[str_i]);  //設定顯示字符
	    }
    }
    if(clock_mode == SET_TIME || clock_mode == SET_ALARM)
    {
        display_min = (setting_sec / 60) % 60;
        display_hr  = (setting_sec / 60) / 60;
        sprintf(current_time_str, "%02d:%02d   ", display_hr,display_min);
        ClearLCD(1);
	    LCD_setCursor(0,1);//設定游標座標
	    for(str_i = 0; str_i < 8; str_i++)
	    {
	    	LCD_Write(current_time_str[str_i]);  //設定顯示字符
	    }
    }

    printf("In %s mode\r\n", (clock_mode == CUR_TIME) ? "CUR_TIME": (clock_mode == SET_TIME) ? "SET_TIME": (clock_mode == SET_ALARM) ? "SET_ALARM":"MATH");
    printf("Time %02d : %02d : %02d\r\n", real_hr, real_min, real_sec);
}

void GPTM_Configuration(void)
{
	GPTM_OutputInitStructure.Channel = TM_CH_3;
	GPTM_OutputInitStructure.OutputMode = TM_OM_PWM1;
	GPTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
	GPTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
	GPTM_OutputInitStructure.Compare = 100;
	GPTM_OutputInitStructure.AsymmetricCompare = 0;
	
	TM_OutputInit(HT_GPTM0, &GPTM_OutputInitStructure);
	
	// GPTM_OutputInitStructure.Channel = TM_CH_3;
	// GPTM_OutputInitStructure.OutputMode = TM_OM_PWM1;
	// GPTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
	// GPTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
	// GPTM_OutputInitStructure.Compare = Period * 1;
	// GPTM_OutputInitStructure.AsymmetricCompare = 0;
	
	// TM_OutputInit(HT_GPTM0, &GPTM_OutputInitStructure);
}


void GPTM_Time_Configuration(float Frequency)
{
    int Period = (SystemCoreClock / 0x10000);
	int DIV = (0x10000/Frequency);
	int cnt;
	
	

	//設每(SystemCoreClock / 0x10000)個Clock一個週期
	//即當除頻值為0x10000時，週期為 1 秒=>頻率為 1 Hz
	//即當除頻值為0x10000/n時，週期為 1/n 秒=>頻率為 n Hz
	GPTM_TimeBaseInitStructure.CounterReload = Period - 1;

	//除頻器直設為(0x10000/Frequency)
	GPTM_TimeBaseInitStructure.Prescaler = DIV - 1;

	GPTM_TimeBaseInitStructure.CounterMode = TM_CNT_MODE_UP;
	GPTM_TimeBaseInitStructure.PSCReloadTime = TM_PSC_RLD_UPDATE;
	TM_TimeBaseInit(HT_GPTM0, &GPTM_TimeBaseInitStructure);

	TM_Cmd(HT_GPTM0, ENABLE);
	
	if(Frequency == 1)
		for(cnt = 0; cnt < DELAY_SHORT;  cnt++);
	else
		for(cnt = 0; cnt < DELAY_MIDDLE; cnt++);
}

void Little_Star(void)
{
	int cnt;
	for(cnt = 0; cnt < DELAY_MIDDLE; cnt++);
		
    while(clock_mode == MATH_EXPRESSION)
    {
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_LA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_LA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
            
            
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
            
            
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_LA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_LA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_SO);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_FA);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_MI);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_RE);
        GPTM_Time_Disable(DELAY_SHORT);
        if(clock_mode != MATH_EXPRESSION) break;
        GPTM_Time_Configuration(FREQ_DO);
        GPTM_Time_Disable(DELAY_MIDDLE);
        if(clock_mode != MATH_EXPRESSION) break;
            
        TM_Cmd(HT_GPTM0, DISABLE);
        GPTM_Time_Disable(DELAY_LONG * 4);
    }
}

void GPTM_Time_Disable(int delay)
{
	int cnt;
	TM_Cmd(HT_GPTM0, DISABLE);
	for(cnt = 0; cnt < delay; cnt++);
}

int TimeBeforeTarget(int target_s)
{
    if ((target_s - abs_sec) >= 0)
        return (target_s - abs_sec);
    else
        return (SECS_A_DAY - target_s + abs_sec);
}



/*********************************************************************************************************//**
  * @brief  LCD Screen Display
  * @retval None
  ***********************************************************************************************************/

void Delay_us(int cnt)
{
	int i;
	for(i=0;i<cnt;i++);
}

void I2C_Write(HT_I2C_TypeDef* I2Cx, u16 slave_address, u8* buffer, u8 BufferSize)
{
	u8 Tx_Index = 0;
	
    /* Send I2C START & I2C slave address for write                                                           */
    I2C_TargetAddressConfig(I2Cx, slave_address, I2C_MASTER_WRITE);

    /* Check on Master Transmitter STA condition and clear it                                                 */
    while (!I2C_CheckStatus(I2Cx, I2C_MASTER_SEND_START));
	
    /* Check on Master Transmitter ADRS condition and clear it                                                */
    while (!I2C_CheckStatus(I2Cx, I2C_MASTER_TRANSMITTER_MODE));
	
    /* Send data                                                                                              */
    while (Tx_Index < BufferSize)
    {
        /* Check on Master Transmitter TXDE condition                                                           */
        while (!I2C_CheckStatus(I2Cx, I2C_MASTER_TX_EMPTY));
        /* Master Send I2C data                                                                                 */
        I2C_SendData(I2Cx, buffer[Tx_Index ++]);
    }
    /* Send I2C STOP condition                                                                                */
    I2C_GenerateSTOP(I2Cx);
    /*wait for BUSBUSY become idle                                                                            */
    while (I2C_ReadRegister(I2Cx, I2C_REGISTER_SR)&0x80000);
}

void I2C_Read(HT_I2C_TypeDef* I2Cx, u16 slave_address, u8* buffer, u8 BufferSize)
{
	u8 Rx_Index = 0;
	
	/* Send I2C START & I2C slave address for read                                                            */
    I2C_TargetAddressConfig(I2Cx, slave_address, I2C_MASTER_READ);

    /* Check on Master Transmitter STA condition and clear it                                                 */
    while (!I2C_CheckStatus(I2Cx, I2C_MASTER_SEND_START));

    /* Check on Master Transmitter ADRS condition and clear it                                                */
    while (!I2C_CheckStatus(I2Cx, I2C_MASTER_RECEIVER_MODE));

    I2C_AckCmd(I2Cx, ENABLE);
    /* Send data                                                                                              */
    while (Rx_Index < BufferSize)
    {

        /* Check on Slave Receiver RXDNE condition                                                              */
        while (!I2C_CheckStatus(I2Cx, I2C_MASTER_RX_NOT_EMPTY));
        /* Store received data on I2C1                                                                          */
        buffer[Rx_Index ++] = I2C_ReceiveData(I2Cx);
        if (Rx_Index == (BufferSize-1))
        {
            I2C_AckCmd(I2Cx, DISABLE);
        }
    }
    /* Send I2C STOP condition                                                                                */
    I2C_GenerateSTOP(I2Cx);
    /*wait for BUSBUSY become idle                                                                            */
    while (I2C_ReadRegister(I2Cx, I2C_REGISTER_SR)&0x80000);
}

void LCD_I2C_1602_4bit_Write(u8 data)
{
	data |= _backlightval;
	
	I2C_Write(LCD_I2C_CH,I2C_SLAVE_ADDRESS,&data,1);

	data |= I2C_1062_E;
	I2C_Write(LCD_I2C_CH,I2C_SLAVE_ADDRESS,&data,1);
	Delay_us(1000);
	
	data &= ~I2C_1062_E;
	I2C_Write(LCD_I2C_CH,I2C_SLAVE_ADDRESS,&data,1);
	Delay_us(50000);
}

void LCD_command(u8 command)
{
	u8 high_4b = command & 0xF0;
	u8 low_4b = (command<<4) & 0xF0;
	
	LCD_I2C_1602_4bit_Write(high_4b);
	LCD_I2C_1602_4bit_Write(low_4b);
}

void LCD_ini(void)
{
	_backlightval=LCD_BACKLIGHT;

	_displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
	Delay_us(200000);

	LCD_I2C_1602_4bit_Write(0x30);
	Delay_us(500000);
	LCD_I2C_1602_4bit_Write(0x30);
	Delay_us(200000);
	LCD_I2C_1602_4bit_Write(0x30);
	Delay_us(200000);
	//printf("33");
	LCD_I2C_1602_4bit_Write(LCD_FUNCTIONSET | LCD_4BITMODE);
	
	LCD_command(LCD_FUNCTIONSET | _displayfunction); 
	
	_displaycontrol = LCD_DISPLAYOFF | LCD_CURSOROFF | LCD_BLINKOFF;
	LCD_command(LCD_DISPLAYCONTROL | _displaycontrol);
	
	LCD_command(LCD_CLEARDISPLAY);
	Delay_us(200000);
	
	LCD_command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
	
	LCD_command(LCD_RETURNHOME);
	Delay_us(200000);
	
	_displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	LCD_command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void LCD_Backlight(u8 enable)
{
	u8 data = 0;
	if(enable)	_backlightval=LCD_BACKLIGHT;
	else				_backlightval=LCD_NOBACKLIGHT;
	data = _backlightval;
	I2C_Write(LCD_I2C_CH,I2C_SLAVE_ADDRESS,&data,1);
}

//也需設定CKCU、AFIO
void I2C_Configuration(void)
{
    /* I2C Master configuration                                                                               */
    I2C_InitTypeDef  I2C_InitStructure;

    I2C_InitStructure.I2C_GeneralCall = DISABLE;
    I2C_InitStructure.I2C_AddressingMode = I2C_ADDRESSING_7BIT;//I2C_ADDRESSING_7BIT 
    I2C_InitStructure.I2C_Acknowledge = DISABLE;
    I2C_InitStructure.I2C_OwnAddress = I2C_MASTER_ADDRESS;
    I2C_InitStructure.I2C_Speed = ClockSpeed;
    I2C_Init(HT_I2C1, &I2C_InitStructure);

	
	I2C_InitStructure.I2C_Acknowledge = DISABLE;
    I2C_InitStructure.I2C_OwnAddress = I2C_SLAVE_ADDRESS;
    //I2C_InitStructure.I2C_Speed = ClockSpeed;
    I2C_Init(HT_I2C0, &I2C_InitStructure);
    /* Enable I2C                                                                                             */
    I2C_Cmd(HT_I2C0 , ENABLE);
	I2C_Cmd(HT_I2C1 , ENABLE);
}

/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************//**
  * @brief  Main program.
  * @retval None
  ***********************************************************************************************************/
void LCD_setCursor(uint8_t col, uint8_t row)
{
	int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (row > LCD_MAX_ROWS)
    {
		row = LCD_MAX_ROWS-1;    // we count rows starting w/0
	}
	LCD_command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void LCD_Write(u8 Data)
{
	u8 high_4b = (Data & 0xF0) | I2C_1062_RS;
	u8 low_4b = ((Data<<4) & 0xF0) | I2C_1062_RS;
	
	LCD_I2C_1602_4bit_Write(high_4b);
	LCD_I2C_1602_4bit_Write(low_4b);
}
int GenRandomMathExpression()
{
    int a, b, result;

    srand(sec_after_boot);

    a = rand() % 99 + 1;
    b = rand() % 99 + 1;

    // a = (sec_after_boot * sec_after_boot) % 99 + 1;
    // b = (sec_after_boot * a) % 99 + 1;

    result = a * b;

    DisplayMathExpression(a, b);
    return result;
}

void DisplayMathExpression(int a, int b)
{
    char math_expression_str[7];
    int str_i;
	sprintf(math_expression_str, "%02d x %02d", a, b);
    ClearLCD(0);
	LCD_setCursor(0,0);//設定游標座標
	for(str_i = 0; str_i < 7; str_i++)
	{
		LCD_Write(math_expression_str[str_i]);  //設定顯示字符
	}
}

void DisplayMathInput(int product)
{
    char product_str[4];
    int str_i;
    sprintf(product_str, "%d", product);
    ClearLCD(1);
	LCD_setCursor(0,1);//設定游標座標
    for(str_i = 0; str_i < 4; str_i++)
    {
        LCD_Write(product_str[str_i]);
    }
}

void ClearLCD(int row)
{
    int i;
    LCD_setCursor(0,row);
    for(i = 0; i < 16; i++) LCD_Write(' ');
}

void Read_Button()
{
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_0, RESET);//切換掃描
    button[1]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_7);
    button[2]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_5);
    button[3]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_6);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_0, SET);//切換掃描

    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_1, RESET);//切換掃描
    button[4]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_7);
    button[5]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_5);
    button[6]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_6);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_1, SET);//切換掃描

    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_2, RESET);//切換掃描
    button[7]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_7);
    button[8]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_5);
    button[9]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_6);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_2, SET);//切換掃描

    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_3, RESET);//切換掃描
    button[10] = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_7);
    button[0]  = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_5);
    button[11] = GPIO_ReadInBit(HT_GPIOA, GPIO_PIN_6);
    GPIO_WriteOutBits(HT_GPIOA, GPIO_PIN_3, SET);//切換掃描
}

int Get_Current_Button()
{
    int i;
    for(i = 0; i < 12; i++)
    {
        if(button[i] == RESET)
        {
            return i;
        }
    }
    return -1;
}

void TestLCD(void)
{
    int i;
	LCD_setCursor(0,0);//設定游標座標
	//for(i=0;i<4;i++,n++){
	LCD_Write('a');//設定顯示字符
	LCD_Write('b');
	LCD_Write('c');
	LCD_Write('d');
	LCD_Write('A');
	LCD_Write('B');
	LCD_Write('C');
	LCD_Write('D');
	//}
	LCD_setCursor(0,1);//設定游標座標
	for(i=0;i<10;i++)
    {
	    LCD_Write('0'+i);//設定顯示字符
	}		
	Delay_us(10000000);
}
