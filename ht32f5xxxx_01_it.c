/*********************************************************************************************************//**
 * @file    MCTM/ComplementaryOutput/ht32f5xxxx_01_it.c
 * @version $Rev:: 2970         $
 * @date    $Date:: 2018-08-03 #$
 * @brief   This file provides all interrupt service routine.
 *************************************************************************************************************
 * @attention
 *
 * Firmware Disclaimer Information
 *
 * 1. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, which is supplied by Holtek Semiconductor Inc., (hereinafter referred to as "HOLTEK") is the
 *    proprietary and confidential intellectual property of HOLTEK, and is protected by copyright law and
 *    other intellectual property laws.
 *
 * 2. The customer hereby acknowledges and agrees that the program technical documentation, including the
 *    code, is confidential information belonging to HOLTEK, and must not be disclosed to any third parties
 *    other than HOLTEK and the customer.
 *
 * 3. The program technical documentation, including the code, is provided "as is" and for customer reference
 *    only. After delivery by HOLTEK, the customer shall use the program technical documentation, including
 *    the code, at their own risk. HOLTEK disclaims any expressed, implied or statutory warranties, including
 *    the warranties of merchantability, satisfactory quality and fitness for a particular purpose.
 *
 * <h2><center>Copyright (C) Holtek Semiconductor Inc. All rights reserved</center></h2>
 ************************************************************************************************************/

/* Includes ------------------------------------------------------------------------------------------------*/
#include "ht32.h"
#include "ht32_board.h"
#include "tlpalarm.h"

/** @addtogroup HT32_Series_Peripheral_Examples HT32 Peripheral Examples
  * @{
  */

/** @addtogroup MCTM_Examples MCTM
  * @{
  */

/** @addtogroup ComplementaryOutput
  * @{
  */


/* Global functions ----------------------------------------------------------------------------------------*/
/*********************************************************************************************************//**
 * @brief   This function handles NMI exception.
 * @retval  None
 ************************************************************************************************************/
void NMI_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles Hard Fault exception.
 * @retval  None
 ************************************************************************************************************/
void HardFault_Handler(void)
{
  #if 1

  static vu32 gIsContinue = 0;
  /*--------------------------------------------------------------------------------------------------------*/
  /* For development FW, MCU run into the while loop when the hardfault occurred.                           */
  /* 1. Stack Checking                                                                                      */
  /*    When a hard fault exception occurs, MCU push following register into the stack (main or process     */
  /*    stack). Confirm R13(SP) value in the Register Window and typing it to the Memory Windows, you can   */
  /*    check following register, especially the PC value (indicate the last instruction before hard fault).*/
  /*    SP + 0x00    0x04    0x08    0x0C    0x10    0x14    0x18    0x1C                                   */
  /*           R0      R1      R2      R3     R12      LR      PC    xPSR                                   */
  while (gIsContinue == 0)
  {
  }
  /* 2. Step Out to Find the Clue                                                                           */
  /*    Change the variable "gIsContinue" to any other value than zero in a Local or Watch Window, then     */
  /*    step out the HardFault_Handler to reach the first instruction after the instruction which caused    */
  /*    the hard fault.                                                                                     */
  /*--------------------------------------------------------------------------------------------------------*/

  #else

  /*--------------------------------------------------------------------------------------------------------*/
  /* For production FW, you shall consider to reboot the system when hardfault occurred.                    */
  /*--------------------------------------------------------------------------------------------------------*/
  NVIC_SystemReset();

  #endif
}

/*********************************************************************************************************//**
 * @brief   This function handles SVCall exception.
 * @retval  None
 ************************************************************************************************************/
void SVC_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles PendSVC exception.
 * @retval  None
 ************************************************************************************************************/
void PendSV_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles SysTick Handler.
 * @retval  None
 ************************************************************************************************************/
void SysTick_Handler(void)
{
}

/*********************************************************************************************************//**
 * @brief   This function handles BFTM0 interrupt.
 * @retval  None
 ************************************************************************************************************/

extern int abs_sec;
extern int temper_L2H_flag;
extern int temper_H2L_flag;
extern int alarm_time_s;
extern int target_time_s;
extern int cur_button;
extern int clock_mode;
extern int setting_sec;
extern int time_set_cursor;
extern int time_set_hr_ten;
extern int correct_password;
extern unsigned int sec_after_boot;
extern void LED_Set_Temperature(int Temperature);
extern void DisplayLCD(void);
extern void ClearLCD(int row);
extern int TimeBeforeTarget(int target_s);
extern int Read_Button(void);
extern int Get_Current_Button(void);
extern int GenRandomMathExpression(void);
extern void DisplayMathExpression(int a, int b);
extern void DisplayMathInput(int product);
int B = 0;
int input_password = 11111;
int input_password_cursor;
int math_reset = 1;
int mode_change_check;
void BFTM0_IRQHandler(void)
{
    if(temper_L2H_flag)
    {
        printf("L2H\r\n");
        B = TimeBeforeTarget(target_time_s);
        B = MAX_BRIGHTNESS_LEVEL - (B * MAX_BRIGHTNESS_LEVEL / TEMP_PREACT_TIME);
        LED_Set_Temperature(B);
    }
    else if(temper_H2L_flag)
    {
        printf("H2L\r\n");
        B = TimeBeforeTarget(target_time_s);
        B = (B * MAX_BRIGHTNESS_LEVEL / TEMP_PREACT_TIME);
        LED_Set_Temperature(B);
    }
    else
    {
        printf("fix temp\r\n");
    }
	
    sec_after_boot++;
	abs_sec++;
	if (abs_sec >= SECS_A_DAY) abs_sec = 0;
	DisplayLCD();
	
    BFTM_ClearFlag(HT_BFTM0);
}

void BFTM1_IRQHandler(void)
{
    if(mode_change_check != clock_mode)
    {
        ClearLCD(0);
        ClearLCD(1);
    }
    mode_change_check = clock_mode;
    // 取得所按下的按鈕
    Read_Button();
    cur_button = Get_Current_Button();

    // 在Terminal顯示，For debugging
    // printf("                       \r\n");
    // if(cur_button == -1)
    // {
    //     printf("No Button be pressed\r\n");
    // }
    // else
    // {
    //     if(cur_button == 10)
    //         printf("Current Button :    *\r\n");
    //     else if(cur_button == 11)
    //         printf("Current Button :    #\r\n");
    //     else
    //         printf("Current Button :  %3d\r\n", cur_button);
    // }

    if(clock_mode == CUR_TIME)      // 正在 目前時間模式
    {
        if(cur_button == 10)        // [*] 鍵，進入設置時間模式
        {
            clock_mode = SET_TIME;
        }
        else if(cur_button == 11)   // [#] 鍵，進入設置鬧鐘模式
        {
            clock_mode = SET_ALARM;
        }
    }
    else if(clock_mode == SET_TIME) // 正在 設定時間模式
    {
        if(cur_button != -1)        // 有按按鍵
        {
            if(cur_button == 11)        // [#] 鍵，確認時間設定，回到目前時間模式
            {
                // 更新 新時間
                abs_sec = setting_sec;
                CLEAR_TIME_SET
                clock_mode = CUR_TIME;
                printf("Set Back CUR_TIME Mode by [#] !!!\r\n");
            }
            else if(cur_button == 10)    // [*]鍵 取消設定時間
            {
                CLEAR_TIME_SET
                clock_mode = CUR_TIME;
                printf("Set Back CUR_TIME Mode by [*] !!!\r\n");
            }
            else                        // 逐位數設定
            {
                if(time_set_cursor == 0 && cur_button <= 2)     // 十小時
                {
                    time_set_hr_ten = cur_button;
                    setting_sec = setting_sec + cur_button * 10 * 60 * 60;
                    time_set_cursor = time_set_cursor + 1;
                }
                else if(time_set_cursor == 1)                   // 一小時
                {
                    if(time_set_hr_ten != 2 || cur_button <= 3)
                    {
                        setting_sec = setting_sec + cur_button * 60 * 60;
                        time_set_cursor = time_set_cursor + 1;
                    }
                }
                else if(time_set_cursor == 2 && cur_button <= 5)// 十分鐘
                {
                    setting_sec = setting_sec + cur_button * 10 * 60;
                    time_set_cursor = time_set_cursor + 1;
                }
                else if(time_set_cursor == 3)                   // 一分鐘
                {
                    setting_sec = setting_sec + cur_button * 60;
                    // 更新 新時間
                    abs_sec = setting_sec;
                    CLEAR_TIME_SET
                    clock_mode = CUR_TIME;
                    printf("Set Back CUR_TIME Mode by [time set end] !!!\r\n");
                }
            }
        }
    }
    else if(clock_mode == SET_ALARM)// 正在 設定鬧鐘模式
    {
        if(cur_button != -1)        // 有按按鍵
        {
            if(cur_button == 10)        // [*] 鍵，刪除鬧鐘，回到目前時間模式
            {
                // 刪除鬧鐘
                alarm_time_s = NO_ALARM;
                CLEAR_TIME_SET
                clock_mode = CUR_TIME;
            }
            else if(cur_button == 11)    // [#]鍵 取消設定時間
            {
                CLEAR_TIME_SET
                clock_mode = CUR_TIME;
            }
            else                        // 逐位數設定
            {
                if(time_set_cursor == 0 && cur_button <= 2)     // 十小時
                {
                    time_set_hr_ten = cur_button;
                    setting_sec = setting_sec + cur_button * 10 * 60 * 60;
                    time_set_cursor = time_set_cursor + 1;
                }
                else if(time_set_cursor == 1)                   // 一小時
                {
                    if(time_set_hr_ten != 2 || cur_button <= 3)
                    {
                        setting_sec = setting_sec + cur_button * 60 * 60;
                        time_set_cursor = time_set_cursor + 1;
                    }
                }
                else if(time_set_cursor == 2 && cur_button <= 5)// 十分鐘
                {
                    setting_sec = setting_sec + cur_button * 10 * 60;
                    time_set_cursor = time_set_cursor + 1;
                }
                else if(time_set_cursor == 3)                   // 一分鐘
                {
                    setting_sec = setting_sec + cur_button * 60;
                    // 更新 新鬧鐘時間
                    alarm_time_s = setting_sec;
                    clock_mode = CUR_TIME;
                    CLEAR_TIME_SET
                }
            }
        }
    }
    else                            // 正在 數學運算式模式
    {
        if(math_reset == 1)
        {
            correct_password = GenRandomMathExpression();
            input_password = 0;
            math_reset = 0;
        }
        if(input_password_cursor <= 3)
        {
            if(cur_button != -1)
            {
                input_password = input_password * 10 + cur_button;
                input_password_cursor += 1;
                DisplayMathInput(input_password);
            }
        }
        else
        {
            if(input_password == correct_password)
            {
                clock_mode = CUR_TIME;
                input_password = 11111;
                math_reset = 1;
            }
            input_password = 0;
            ClearLCD(1);
            input_password_cursor = 0;
        }
    }
    cur_button = -1;
    
    BFTM_ClearFlag(HT_BFTM1);
}

/*********************************************************************************************************//**
 * @brief   This function handles BFTM1 interrupt.
 * @retval  None
 ************************************************************************************************************/


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
