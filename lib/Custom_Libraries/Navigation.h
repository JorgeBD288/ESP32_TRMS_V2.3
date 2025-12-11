#pragma once
#include <lvgl.h>
#include "MotorControl.h"
#include "PID_Parameters.h"
#include "IRControl.h"

void HandleIRNavigation(IRNavKey nav);
void HandleDeltaSlider(int delta);
void HandleNumericDigit(int digit);
void HandleNumericMinus();

void SetupScreen1Nav();
void SetupScreen2Nav();
void SetupScreen3Nav();
void SetupScreen4Nav();
void SetupScreen5Nav();
void SetupScreen6Nav();
void SetupScreen7Nav();
void SetupScreen8Nav();
void SetupScreen9Nav();