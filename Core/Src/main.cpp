#include "main.h"
#include "a4988.hpp"
#include "dc_motor.hpp"
#include "encoder.hpp"
#include "gpio.h"
#include "h_bridge.hpp"
#include "l298n.hpp"
#include "pid.hpp"
#include "pwm_device.hpp"
#include "servo.hpp"
#include "system_clock.h"
#include "tim.h"
#include "usart.h"

using namespace Motors;
using namespace Utility;

void test_servo()
{
    MX_GPIO_Init();
    MX_TIM1_Init();

    PWMDevice pwm_device{&htim1, TIM_CHANNEL_2, 0UL, htim1.Init.Period, 0.0F, 180.0F};

    Servo servo{.angle_to_voltage = [](float const angle) noexcept { return angle; },
                .pwm_device = std::move(pwm_device)};

    while (true) {
        servo.run_sequence(0.0F, 45.0F, 90.0F, 135.0F, 180.0F, 135.0F, 90.0F, 45.0F, 0.0F);
    }
}

void test_stepper_driver()
{
    MX_GPIO_Init();

    PWMDevice pwm_device(&htim1, TIM_CHANNEL_4, 0UL, 1UL, 0.0F, 3.3F);

    A4988 a4988{A4988_DIR_GPIO_Port,
                A4988_MS1_Pin,
                A4988_MS2_Pin,
                A4988_MS3_Pin,
                A4988_RESET_Pin,
                A4988_SLEEP_Pin,
                A4988_STEP_Pin,
                A4988_DIR_Pin,
                A4988_ENABLE_Pin};

    while (true) {
        a4988.trigger_next_step(A4988::Direction::FORWARD, A4988::StepRes::FULL);
        HAL_Delay(50UL);
        a4988.trigger_next_step(A4988::Direction::BACKWARD, A4988::StepRes::FULL);
        HAL_Delay(50UL);
    }
}

static auto sampling_timer_elapsed{false};

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM2) {
        sampling_timer_elapsed = true;
        HAL_TIM_Base_Start_IT(htim);
    }
}

void test_motor_driver()
{
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();

    PID<float> regulator{.proportion_gain = 1.0F,
                         .integral_gain = 0.0F,
                         .derivative_gain = 0.0F,
                         .control_gain = 0.0F,
                         .saturation = 6.0F};

    PWMDevice pwm_device{&htim1, TIM_CHANNEL_2, 0UL, htim1.Init.Period, 0.0F, 6.0F};

    HBridge h_bridge{.pwm_device = std::move(pwm_device),
                     .gpio = L298N_IN1_GPIO_Port,
                     .pin_left = L298N_IN1_Pin,
                     .pin_right = L298N_IN2_Pin};

    CNTDevice cnt_device{&htim3, htim3.Init.Period};

    Encoder encoder{.cnt_device = std::move(cnt_device), .counts_per_pulse = 1UL, .pulses_per_360 = 52UL};

    DCMotor motor_driver{
        .regulator = std::move(regulator),
        .motor = std::move(h_bridge),
        .encoder = std::move(encoder),
        .speed_to_voltage = [](float const speed) noexcept { return 0.0F; },
        .speed_to_direction = [](float const speed) noexcept { return HBridge::Direction::SOFT_STOP; }};

    HAL_TIM_Base_Start_IT(&htim2);

    while (true) {
        if (sampling_timer_elapsed) {
            motor_driver(300.0F, 0.100F);
            sampling_timer_elapsed = false;
        }
    }
}

int main()
{
    HAL_Init();
    SystemClock_Config();

    return 0;
}
