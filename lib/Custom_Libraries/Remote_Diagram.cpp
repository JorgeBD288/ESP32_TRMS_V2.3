#include "Remote_Diagram.h"
#include "Navigation.h"
#include "IRControl.h"
#include <Arduino.h>
#include <stdint.h>
#include "ui.h"  


// Clase y definiciones para el modo de aprendizaje automático

enum class LearnType {
    NONE,
    DIGIT,
    NAV,
    PLUS,
    MINUS,
    POWER
};

static LearnType s_learningType  = LearnType::NONE;
static int       s_learningDigit = -1;       // 0–9 si estamos en DIGIT
static IRNavKey  s_learningNav   = IRNavKey::NONE;


// ======================================================================
// OBJETOS GLOBALES
// ======================================================================

lv_obj_t * ui_RemoteBtn1   = nullptr;
lv_obj_t * ui_RemoteBtn2   = nullptr;
lv_obj_t * ui_RemoteBtn3   = nullptr;
lv_obj_t * ui_RemoteBtn4   = nullptr;
lv_obj_t * ui_RemoteBtn5   = nullptr;
lv_obj_t * ui_RemoteBtn6   = nullptr;
lv_obj_t * ui_RemoteBtn7   = nullptr;
lv_obj_t * ui_RemoteBtn8   = nullptr;
lv_obj_t * ui_RemoteBtn9   = nullptr;
lv_obj_t * ui_RemoteBtn0   = nullptr;

lv_obj_t * ui_RemoteBtnEnter  = nullptr;
lv_obj_t * ui_RemoteBtnUp     = nullptr;
lv_obj_t * ui_RemoteBtnDown   = nullptr;
lv_obj_t * ui_RemoteBtnLeft   = nullptr;
lv_obj_t * ui_RemoteBtnRight  = nullptr;

lv_obj_t * ui_RemoteBtnPlus   = nullptr;
lv_obj_t * ui_RemoteBtnMinus  = nullptr;
lv_obj_t * ui_RemoteBtnPower  = nullptr;

// ======================================================================
// CONSTANTES DE GEOMETRÍA (coordenadas relativas a la imagen)
// ======================================================================

// Números → círculos de radio 15 px (usaremos bounding box cuadrado)
static const int REMOTE_NUM_R = 15;

// Centros (x,y)
static const int REMOTE_1_CX =  15;
static const int REMOTE_1_CY =  23;
static const int REMOTE_2_CX =  66;
static const int REMOTE_2_CY =  23;
static const int REMOTE_3_CX = 116;
static const int REMOTE_3_CY =  23;

static const int REMOTE_4_CX =  15;
static const int REMOTE_4_CY =  61;
static const int REMOTE_5_CX =  66;
static const int REMOTE_5_CY =  61;
static const int REMOTE_6_CX = 116;
static const int REMOTE_6_CY =  61;

static const int REMOTE_7_CX =  15;
static const int REMOTE_7_CY = 98;
static const int REMOTE_8_CX =  66;
static const int REMOTE_8_CY = 98;
static const int REMOTE_9_CX = 116;
static const int REMOTE_9_CY = 98;

static const int REMOTE_0_CX =  66;
static const int REMOTE_0_CY = 135;

// ENTER (rectángulo)
static const int REMOTE_ENTER_X1 = 226;
static const int REMOTE_ENTER_Y1 =  48;
static const int REMOTE_ENTER_X2 = 276;
static const int REMOTE_ENTER_Y2 =  90;

// Flechas (rectángulos)
// Izquierda: TL (162,50), BR (212,90)
static const int REMOTE_LEFT_X1  = 162;
static const int REMOTE_LEFT_Y1  =  50;
static const int REMOTE_LEFT_X2  = 212;
static const int REMOTE_LEFT_Y2  =  90;

// Derecha: TL (239,50), BR (290,90)
static const int REMOTE_RIGHT_X1 = 289;
static const int REMOTE_RIGHT_Y1 = 50;
static const int REMOTE_RIGHT_X2 = 339;
static const int REMOTE_RIGHT_Y2 = 88;

// Arriba: TL (230,0), BR (273,37)
static const int REMOTE_UP_X1    = 230;
static const int REMOTE_UP_Y1    =   0;
static const int REMOTE_UP_X2    = 273;
static const int REMOTE_UP_Y2    =  37;

// Abajo: TL (229,100), BR (272,139)
static const int REMOTE_DOWN_X1  = 229;
static const int REMOTE_DOWN_Y1  = 100;
static const int REMOTE_DOWN_X2  = 272;
static const int REMOTE_DOWN_Y2  = 139;

// Cuadro "+"
static const int REMOTE_PLUS_X1  = 383;
static const int REMOTE_PLUS_Y1  =  60;
static const int REMOTE_PLUS_X2  = 420;
static const int REMOTE_PLUS_Y2  =  91;

// Cuadro "−"
static const int REMOTE_MINUS_X1 = 383;
static const int REMOTE_MINUS_Y1 = 100;
static const int REMOTE_MINUS_X2 = 421;
static const int REMOTE_MINUS_Y2 = 130;

// Power (círculo centro 401,28 radio 20 → bounding box)
static const int REMOTE_PWR_R    = 20;
static const int REMOTE_PWR_CX   = 401;
static const int REMOTE_PWR_CY   =  28;

// ======================================================================
// HELPERS: creación de botones invisibles
// ======================================================================

static lv_obj_t * create_circle_button(lv_obj_t * parent, int cx, int cy, int r)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, cx - r, cy - r);
    lv_obj_set_size(btn, 2 * r, 2 * r);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);

    return btn;
}

static lv_obj_t * create_rect_button(lv_obj_t * parent, int x1, int y1, int x2, int y2)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x1, y1);
    lv_obj_set_size(btn, x2 - x1, y2 - y1);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);

    return btn;
}


// Mostrar en la label el botón seleccionado, con su código IR correspondiente

static void show_selected_label(const char *name, const char *extra = nullptr)
{
    if (!ui_ButtonSelected) return;

    char buf[96];

    if (extra) {
        snprintf(buf, sizeof(buf), "Boton %s seleccionado\n%s", name, extra);
    } else {
        snprintf(buf, sizeof(buf), "Boton %s seleccionado", name);
    }

    lv_label_set_text(ui_ButtonSelected, buf);
    lv_obj_clear_flag(ui_ButtonSelected, LV_OBJ_FLAG_HIDDEN);
}


// ======================================================================
// EVENTOS
// ======================================================================

static void remote_digit_event(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    void *ud = lv_event_get_user_data(e);
    int digit = (int)(intptr_t)ud;

    s_learningType  = LearnType::DIGIT;
    s_learningDigit = digit;
    s_learningNav   = IRNavKey::NONE;

    char name[8];
    snprintf(name, sizeof(name), "%d", digit);

    show_selected_label(name, "Esperando codigo IR...");
}


static void remote_nav_event(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    void *ud = lv_event_get_user_data(e);
    int key_i = (int)(intptr_t)ud;
    IRNavKey key = static_cast<IRNavKey>(key_i);

    s_learningType  = LearnType::NAV;
    s_learningDigit = -1;
    s_learningNav   = key;

    const char *name = "?";
    switch (key) {
        case IRNavKey::LEFT:  name = "Izquierda"; break;
        case IRNavKey::RIGHT: name = "Derecha";   break;
        case IRNavKey::UP:    name = "Arriba";    break;
        case IRNavKey::DOWN:  name = "Abajo";     break;
        case IRNavKey::ENTER: name = "ENTER";     break;
        default: break;
    }

    show_selected_label(name, "Esperando codigo IR...");
}



static void remote_delta_event(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    void *ud = lv_event_get_user_data(e);
    int delta = (int)(intptr_t)ud;

    s_learningDigit = -1;
    s_learningNav   = IRNavKey::NONE;

    if (delta > 0) {
        s_learningType = LearnType::PLUS;
        show_selected_label("+", "Esperando codigo IR...");
    } else if (delta < 0) {
        s_learningType = LearnType::MINUS;
        show_selected_label("-", "Esperando codigo IR...");
    } else {
        s_learningType = LearnType::NONE;
    }
}


static void remote_power_event(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    s_learningType  = LearnType::POWER;
    s_learningDigit = -1;
    s_learningNav   = IRNavKey::NONE;

    show_selected_label("POWER", "Esperando codigo IR...");
}


// ======================================================================
// INICIALIZACIÓN PÚBLICA
// ======================================================================

void RemoteDiagram_Init(lv_obj_t *remoteImg)
{
    // Ocultar la label al inicio
    if (ui_ButtonSelected) {
        lv_obj_add_flag(ui_ButtonSelected, LV_OBJ_FLAG_HIDDEN);
    }

    // --- NÚMEROS ---
    ui_RemoteBtn1 = create_circle_button(remoteImg, REMOTE_1_CX, REMOTE_1_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn1, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)1);

    ui_RemoteBtn2 = create_circle_button(remoteImg, REMOTE_2_CX, REMOTE_2_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn2, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)2);

    ui_RemoteBtn3 = create_circle_button(remoteImg, REMOTE_3_CX, REMOTE_3_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn3, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)3);

    ui_RemoteBtn4 = create_circle_button(remoteImg, REMOTE_4_CX, REMOTE_4_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn4, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)4);

    ui_RemoteBtn5 = create_circle_button(remoteImg, REMOTE_5_CX, REMOTE_5_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn5, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)5);

    ui_RemoteBtn6 = create_circle_button(remoteImg, REMOTE_6_CX, REMOTE_6_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn6, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)6);

    ui_RemoteBtn7 = create_circle_button(remoteImg, REMOTE_7_CX, REMOTE_7_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn7, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)7);

    ui_RemoteBtn8 = create_circle_button(remoteImg, REMOTE_8_CX, REMOTE_8_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn8, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)8);

    ui_RemoteBtn9 = create_circle_button(remoteImg, REMOTE_9_CX, REMOTE_9_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn9, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)9);

    ui_RemoteBtn0 = create_circle_button(remoteImg, REMOTE_0_CX, REMOTE_0_CY, REMOTE_NUM_R);
    lv_obj_add_event_cb(ui_RemoteBtn0, remote_digit_event, LV_EVENT_ALL, (void*)(intptr_t)0);

    // --- ENTER ---
    ui_RemoteBtnEnter = create_rect_button(remoteImg,
                                           REMOTE_ENTER_X1, REMOTE_ENTER_Y1,
                                           REMOTE_ENTER_X2, REMOTE_ENTER_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnEnter, remote_nav_event, LV_EVENT_ALL,
                        (void*)(intptr_t)static_cast<int>(IRNavKey::ENTER));

    // --- FLECHAS ---
    ui_RemoteBtnLeft = create_rect_button(remoteImg,
                                          REMOTE_LEFT_X1, REMOTE_LEFT_Y1,
                                          REMOTE_LEFT_X2, REMOTE_LEFT_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnLeft, remote_nav_event, LV_EVENT_ALL,
                        (void*)(intptr_t)static_cast<int>(IRNavKey::LEFT));

    ui_RemoteBtnRight = create_rect_button(remoteImg,
                                           REMOTE_RIGHT_X1, REMOTE_RIGHT_Y1,
                                           REMOTE_RIGHT_X2, REMOTE_RIGHT_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnRight, remote_nav_event, LV_EVENT_ALL,
                        (void*)(intptr_t)static_cast<int>(IRNavKey::RIGHT));

    ui_RemoteBtnUp = create_rect_button(remoteImg,
                                        REMOTE_UP_X1, REMOTE_UP_Y1,
                                        REMOTE_UP_X2, REMOTE_UP_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnUp, remote_nav_event, LV_EVENT_ALL,
                        (void*)(intptr_t)static_cast<int>(IRNavKey::UP));

    ui_RemoteBtnDown = create_rect_button(remoteImg,
                                          REMOTE_DOWN_X1, REMOTE_DOWN_Y1,
                                          REMOTE_DOWN_X2, REMOTE_DOWN_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnDown, remote_nav_event, LV_EVENT_ALL,
                        (void*)(intptr_t)static_cast<int>(IRNavKey::DOWN));

    // --- + / - (sliders) ---
    ui_RemoteBtnPlus = create_rect_button(remoteImg,
                                          REMOTE_PLUS_X1, REMOTE_PLUS_Y1,
                                          REMOTE_PLUS_X2, REMOTE_PLUS_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnPlus, remote_delta_event, LV_EVENT_ALL,
                        (void*)(intptr_t)+10);   // mismo delta que usas con IR real

    ui_RemoteBtnMinus = create_rect_button(remoteImg,
                                           REMOTE_MINUS_X1, REMOTE_MINUS_Y1,
                                           REMOTE_MINUS_X2, REMOTE_MINUS_Y2);
    lv_obj_add_event_cb(ui_RemoteBtnMinus, remote_delta_event, LV_EVENT_ALL,
                        (void*)(intptr_t)-10);

    // --- POWER ---
    ui_RemoteBtnPower = create_circle_button(remoteImg,
                                             REMOTE_PWR_CX, REMOTE_PWR_CY,
                                             REMOTE_PWR_R);
    lv_obj_add_event_cb(ui_RemoteBtnPower, remote_power_event, LV_EVENT_ALL, nullptr);

    Serial.println("[RemoteDiagram] Inicializado");
}

bool RemoteDiagram_HandleIRLearn(const IRControlEvent &ev)
{
    // Si no estamos aprendiendo nada, no hacemos nada
    if (s_learningType == LearnType::NONE) {
        return false;
    }

    if (!ev.hasEvent) return false;

    uint32_t cmd = ev.command;

    // Ignorar ruido / cero, igual que en IRControl
    if (cmd == 0x0 || cmd == 0x40) {
        return false;
    }

    // -----------------------------
    // Asignar según el tipo
    // -----------------------------
    const char *btnName = "?";

    switch (s_learningType) {
        case LearnType::DIGIT: {
            if (s_learningDigit < 0 || s_learningDigit > 9) break;
            IRControl_setDigitCode((uint8_t)s_learningDigit, cmd);

            static char name[8];
            snprintf(name, sizeof(name), "%d", s_learningDigit);
            btnName = name;
            break;
        }

        case LearnType::NAV: {
            if (s_learningNav == IRNavKey::NONE) break;
            IRControl_setNavCode(s_learningNav, cmd);

            switch (s_learningNav) {
                case IRNavKey::LEFT:  btnName = "Izquierda"; break;
                case IRNavKey::RIGHT: btnName = "Derecha";   break;
                case IRNavKey::UP:    btnName = "Arriba";    break;
                case IRNavKey::DOWN:  btnName = "Abajo";     break;
                case IRNavKey::ENTER: btnName = "ENTER";     break;
                default: break;
            }
            break;
        }

        case LearnType::PLUS: {
            IRControl_setPlusCode(cmd);
            btnName = "+";
            break;
        }

        case LearnType::MINUS: {
            IRControl_setMinusCode(cmd);
            btnName = "-";
            break;
        }

        case LearnType::POWER: {
            IRControl_setPowerCode(cmd);
            btnName = "POWER";
            break;
        }

        case LearnType::NONE:
        default:
            break;
    }

    // Mensaje en la label
    char extra[64];
    snprintf(extra, sizeof(extra), "Codigo 0x%lX asignado al boton %s",
             (unsigned long)cmd, btnName);

    show_selected_label(btnName, extra);

    // Salimos de modo aprendizaje
    s_learningType  = LearnType::NONE;
    s_learningDigit = -1;
    s_learningNav   = IRNavKey::NONE;

    // Este evento IR se considera "consumido"
    return true;
}
