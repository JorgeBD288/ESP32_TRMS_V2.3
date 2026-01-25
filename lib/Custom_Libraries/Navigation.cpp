#include "Navigation.h"
#include "ui.h"
#include "Ang_Select.h"

extern lv_group_t * g_navGroup;
extern lv_style_t   style_focus;

// Botones/zona PID/TRMS de la Screen5
extern lv_obj_t * ui_BtnPIDZone;
extern lv_obj_t * ui_BtnTRMSZone;
extern lv_obj_t * ui_Button15;

// Label(es) aceptadas como entrada numérica
extern lv_obj_t * ui_Label64;  // A. vertical
extern lv_obj_t * ui_Label65;  // A. horizontal

// Buffer para ir acumulando los dígitos escritos
static char      s_numEditBuffer[16] = "";   // magnitud (máx. 3 dígitos, pero sobra espacio)
static lv_obj_t *s_numEditTarget     = nullptr;
static bool      s_numNegative       = false;

// ----------------------------
// Modo precisión para sliders
// ----------------------------
static bool      s_fineModeActive = false;
static lv_obj_t* s_fineSlider     = nullptr;
static int       s_origMin        = 0;
static int       s_origMax        = 0;

// Estilo rojo para indicar precisión
static lv_style_t s_styleFineInd;
static lv_style_t s_styleFineKnob;
static bool       s_styleFineInit = false;

// ============================
// Navegación con IR (flechas + ENTER)
// ============================

/**
 * @brief 
 * Maneja la navegación en la interfaz
 * según la tecla de navegación recibida.
 * @note
  Mueve el foco entre los objetos
  del grupo de navegación global (g_navGroup)
  o simula un click/enter en el objeto enfocado,
  dependiendo de la tecla recibida.
 * @param nav 
 */

void HandleIRNavigation(IRNavKey nav)
{
    if (g_navGroup == nullptr) return;

    switch (nav) {
        case IRNavKey::LEFT:
        case IRNavKey::UP:
            lv_group_focus_prev(g_navGroup);
            break;

        case IRNavKey::RIGHT:
        case IRNavKey::DOWN:
            lv_group_focus_next(g_navGroup);
            break;
        
        case IRNavKey::ENTER: {
            lv_obj_t * focused = lv_group_get_focused(g_navGroup);
            if (focused == nullptr) break;

            // Caso especial: commit de ángulos en Label64/65
            if (focused == ui_Label64 || focused == ui_Label65) {

                // Si no hay edición activa para esta label, no hacemos nada
                if (s_numEditTarget == focused && s_numEditBuffer[0] != '\0') {

                    int magnitude = atoi(s_numEditBuffer);
                    int value     = s_numNegative ? -magnitude : magnitude;

                    // Limitar a [-180, 180]
                    if (value > 180)  value = 180;
                    if (value < -180) value = -180;

                    float newAngle = (float)value;

                    // Partimos de los valores actuales
                    float currH = AngSelect_GetRefHorizontal();
                    float currV = AngSelect_GetRefVertical();

                    if (focused == ui_Label64) {
                        // Editamos vertical
                        currV = newAngle;
                    } else {
                        // Editamos horizontal
                        currH = newAngle;
                    }

                    // Esto actualiza g_refH/g_refV, labels y dibuja el punto con líneas
                    AngSelect_ApplyRefFromAngles(currH, currV);

                    // Salimos del modo edición
                    s_numEditBuffer[0] = '\0';
                    s_numNegative      = false;
                    s_numEditTarget    = nullptr;
                }

                // Para las labels de ángulo, NO simulamos click extra
                break;
            }

            // Toggle precisión para sliders
            if (lv_obj_get_class(focused) == &lv_slider_class) {
                if (s_fineModeActive && s_fineSlider == focused) FineMode_Exit();
                else FineMode_Enter(focused);
                break;
            }

            // Resto de objetos: comportamiento normal (click)
            lv_event_send(focused, LV_EVENT_CLICKED,  NULL);
            lv_event_send(focused, LV_EVENT_PRESSED,  NULL);
            lv_event_send(focused, LV_EVENT_RELEASED, NULL);
            break;
        }

        default:
            break;
    }
}

/**
 * @brief 
 * Maneja el ajuste del valor del slider
 * actualmente enfocado según el delta recibido.
 * @note
  Si el objeto enfocado es un slider,
  ajusta su valor en función del delta proporcionado.
  También actualiza las variables PID correspondientes
  si el slider está vinculado a un parámetro PID.
 * @param delta 
 */

void HandleDeltaSlider(int delta)
{
    if (delta == 0) return;

    lv_obj_t * focused = lv_group_get_focused(g_navGroup);
    if (focused == nullptr) return;

    //Código para detetectar en que modo de incremento de slider estamos, si en el normal o en el de precisión
    if (s_fineModeActive && focused == s_fineSlider) {
    if (delta > 0) delta = 1;
    else if (delta < 0) delta = -1;
    }

    // 1) Caso especial: sliders de motor principal / rotor de cola
    if (focused == ui_MotorPrincipal) {
        Registro_MP += delta;
        lv_slider_set_value(focused, Registro_MP, LV_ANIM_ON);
        MotorControl_update(Registro_MP, Registro_RDC);
        return;
    }

    if (focused == ui_RotorDeCola) {
        Registro_RDC += delta;
        lv_slider_set_value(focused, Registro_RDC, LV_ANIM_ON);
        MotorControl_update(Registro_MP, Registro_RDC);
        return;
    }

    // 2) Si no es ninguno de esos, comprobamos que sea un slider LVGL
    if (lv_obj_get_class(focused) != &lv_slider_class) {
        return;
    }

    // Si estamos sobre el slider de control brillo, cogemos el valor de este y actualizamos su valor
    if (focused == ui_ControlDeBrillo) {
        brightness_Control();
    }

    // Ajustar valor del slider seleccionado
    int value = lv_slider_get_value(focused) + delta;
    int min   = lv_slider_get_min_value(focused);
    int max   = lv_slider_get_max_value(focused);
    value = constrain(value, min, max);

    lv_slider_set_value(focused, value, LV_ANIM_ON);

    // 3) Mapear cada slider a su variable en g_pidCurr
    //    (ajusta los nombres de los campos si no coinciden exactamente)

    if (focused == ui_SliderKpvv) {
        g_pidCurr.KpvvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kpvv,  g_pidMin.KpvvMin,  g_pidCurr.KpvvCurr,  g_pidMax.KpvvMax);
    }
    else if (focused == ui_SliderKpvh) {
        g_pidCurr.KpvhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kpvh,  g_pidMin.KpvhMin,  g_pidCurr.KpvhCurr,  g_pidMax.KpvhMax);
    }
    else if (focused == ui_SliderKivv) {
        g_pidCurr.KivvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kivv,  g_pidMin.KivvMin,  g_pidCurr.KivvCurr,  g_pidMax.KivvMax);
    }
    else if (focused == ui_SliderKivh) {
        g_pidCurr.KivhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kivh,  g_pidMin.KivhMin,  g_pidCurr.KivhCurr,  g_pidMax.KivhMax);
    }
    else if (focused == ui_SliderKdvv) {
        g_pidCurr.KdvvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kdvv,  g_pidMin.KdvvMin,  g_pidCurr.KdvvCurr,  g_pidMax.KdvvMax);
    }
    else if (focused == ui_SliderKdvh) {
        g_pidCurr.KdvhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kdvh,  g_pidMin.KdvhMin,  g_pidCurr.KdvhCurr,  g_pidMax.KdvhMax);
    }
    else if (focused == ui_SliderKphv) {
        g_pidCurr.KphvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kphv,  g_pidMin.KphvMin,  g_pidCurr.KphvCurr,  g_pidMax.KphvMax);
    }
    else if (focused == ui_SliderKphh) {
        g_pidCurr.KphhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kphh,  g_pidMin.KphhMin,  g_pidCurr.KphhCurr,  g_pidMax.KphhMax);
    }
    else if (focused == ui_SliderKihv) {
        g_pidCurr.KihvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kihv,  g_pidMin.KihvMin,  g_pidCurr.KihvCurr,  g_pidMax.KihvMax);
    }
    else if (focused == ui_SliderKihh) {
        g_pidCurr.KihhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kihh,  g_pidMin.KihhMin,  g_pidCurr.KihhCurr,  g_pidMax.KihhMax);
    }
    else if (focused == ui_SliderKdhv) {
        g_pidCurr.KdhvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kdhv,  g_pidMin.KdhvMin,  g_pidCurr.KdhvCurr,  g_pidMax.KdhvMax);
    }
    else if (focused == ui_SliderKdhh) {
        g_pidCurr.KdhhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Kdhh,  g_pidMin.KdhhMin,  g_pidCurr.KdhhCurr,  g_pidMax.KdhhMax);
    }
    else if (focused == ui_SliderIsatvv) {
        g_pidCurr.IsatvvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Isatvv, g_pidMin.IsatvvMin, g_pidCurr.IsatvvCurr, g_pidMax.IsatvvMax);
    }
    else if (focused == ui_SliderIsatvh) {
        g_pidCurr.IsatvhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Isatvh, g_pidMin.IsatvhMin, g_pidCurr.IsatvhCurr, g_pidMax.IsatvhMax);
    }
    else if (focused == ui_SliderIsathv) {
        g_pidCurr.IsathvCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Isathv, g_pidMin.IsathvMin, g_pidCurr.IsathvCurr, g_pidMax.IsathvMax);
    }
    else if (focused == ui_SliderIsathh) {
        g_pidCurr.IsathhCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Isathh, g_pidMin.IsathhMin, g_pidCurr.IsathhCurr, g_pidMax.IsathhMax);
    }
    else if (focused == ui_SliderUvmax) {
        g_pidCurr.UvmaxCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Uvmax, g_pidMin.UvmaxMin, g_pidCurr.UvmaxCurr, g_pidMax.UvmaxMax);
    }
    else if (focused == ui_SliderUhmax) {
        g_pidCurr.UhmaxCurr = value / 10.0f;
        PID_UpdateParamLabel(ui_Uhmax, g_pidMin.UhmaxMin, g_pidCurr.UhmaxCurr, g_pidMax.UhmaxMax);
    }

}
/*
static void ApplyNumericBufferToAngle(lv_obj_t *focused)
{
    if (s_numEditBuffer[0] == '\0') return;   // nada que aplicar

    int magnitude = atoi(s_numEditBuffer);    // 0–999 (antes de limitar)
    int value     = s_numNegative ? -magnitude : magnitude;

    // Limitar a [-180, 180]
    if (value > 180)  value = 180;
    if (value < -180) value = -180;

    // Reajustar signo y buffer según el valor limitado
    if (value < 0) {
        s_numNegative = true;
        magnitude     = -value;
    } else {
        s_numNegative = false;
        magnitude     = value;
    }

    snprintf(s_numEditBuffer, sizeof(s_numEditBuffer), "%d", magnitude);

    float fvalue = (float)value;

    if (focused == ui_Label64) {
        // A. vertical
        AngSelect_SetRefVertical(fvalue);
    } else if (focused == ui_Label65) {
        // A. horizontal
        AngSelect_SetRefHorizontal(fvalue);
    }
}
*/

// ============================
// Entrada numérica con dígitos (preview)
// ============================

/**
 * @brief 
 * Maneja la entrada de un dígito numérico
 * para la label actualmente enfocada.
 * @note
  Si la label enfocada es una de las aceptadas
  para entrada numérica (ui_Label64 o ui_Label65),
  acumula el dígito en un buffer y actualiza
  el texto de la label para mostrar un preview
  del valor ingresado, respetando el signo
  y el rango permitido [-180, 180].
 * @param digit 
 */

void HandleNumericDigit(int digit)
{
    if (digit < 0 || digit > 9) return;
    if (g_navGroup == nullptr)  return;

    lv_obj_t * focused = lv_group_get_focused(g_navGroup);
    if (focused == nullptr) return;

    // Solo en estas labels
    if (focused != ui_Label64 && focused != ui_Label65) {
        return;
    }

    // Si cambiamos de label, empezamos edición nueva
    if (s_numEditTarget != focused) {
        s_numEditTarget    = focused;
        s_numEditBuffer[0] = '\0';
        s_numNegative      = false;
    }

    size_t len = strlen(s_numEditBuffer);

    // Límite de 3 dígitos → si lo superamos, empezamos número nuevo (manteniendo signo)
    if (len >= 3) {
        s_numEditBuffer[0] = '\0';
        len = 0;
    }

    // Añadir dígito
    s_numEditBuffer[len]     = '0' + digit;
    s_numEditBuffer[len + 1] = '\0';

    // Convertimos a valor con signo
    int magnitude = atoi(s_numEditBuffer);
    int value     = s_numNegative ? -magnitude : magnitude;

    // Limitar a [-180, 180] también en preview
    if (value > 180)  value = 180;
    if (value < -180) value = -180;

    // Ajustar buffer y signo según el valor limitado
    if (value < 0) {
        s_numNegative = true;
        magnitude     = -value;
    } else {
        s_numNegative = false;
        magnitude     = value;
    }
    snprintf(s_numEditBuffer, sizeof(s_numEditBuffer), "%d", magnitude);

    // Actualizar texto de la label (solo preview, sin tocar g_ref)
    char txt[48];
    if (focused == ui_Label64) {
        snprintf(txt, sizeof(txt), "A. vertical = %d º", value);
    } else {
        snprintf(txt, sizeof(txt), "A. horizontal = %d º", value);
    }
    lv_label_set_text(focused, txt);
}

// ============================
// Manejo del signo "-"
// ============================

/**
 * @brief 
 * Maneja la entrada del signo menos
 * para la label actualmente enfocada.
 * @note
  Si la label enfocada es una de las aceptadas
  para entrada numérica (ui_Label64 o ui_Label65),
  alterna el signo del número en edición
  y actualiza el texto de la label para mostrar
  un preview del valor con el nuevo signo,
  respetando el rango permitido [-180, 180].
 */

void HandleNumericMinus()
{
    if (g_navGroup == nullptr) return;

    lv_obj_t * focused = lv_group_get_focused(g_navGroup);
    if (focused == nullptr) return;

    if (focused != ui_Label64 && focused != ui_Label65) {
        return;
    }

    // Si cambiamos de label, nueva edición
    if (s_numEditTarget != focused) {
        s_numEditTarget    = focused;
        s_numEditBuffer[0] = '\0';
        s_numNegative      = false;
    }

    // Alternar signo
    s_numNegative = !s_numNegative;

    // Si ya había dígitos, actualizar preview
    if (s_numEditBuffer[0] != '\0') {
        int magnitude = atoi(s_numEditBuffer);
        int value     = s_numNegative ? -magnitude : magnitude;

        // Limitar a [-180, 180]
        if (value > 180)  value = 180;
        if (value < -180) value = -180;

        if (value < 0) {
            s_numNegative = true;
            magnitude     = -value;
        } else {
            s_numNegative = false;
            magnitude     = value;
        }
        snprintf(s_numEditBuffer, sizeof(s_numEditBuffer), "%d", magnitude);

        char txt[48];
        if (focused == ui_Label64) {
            snprintf(txt, sizeof(txt), "A. vertical = %d º", value);
        } else {
            snprintf(txt, sizeof(txt), "A. horizontal = %d º", value);
        }
        lv_label_set_text(focused, txt);
    }
}

/**
 * @brief
 * Inicializa los estilos rojos
 * @note
  Crea los estilos LVGL
  para el modo de precisión de sliders,
  configurando los colores adecuados.
  Solo se ejecuta una vez. 
 */

static void FineStyle_InitOnce()
{
    if (s_styleFineInit) return;
    s_styleFineInit = true;

    lv_style_init(&s_styleFineInd);
    lv_style_set_bg_color(&s_styleFineInd, lv_color_make(255, 0, 0)); // indicador rojo

    lv_style_init(&s_styleFineKnob);
    lv_style_set_bg_color(&s_styleFineKnob, lv_color_make(255, 0, 0)); // knob rojo
}

/**
 * @brief 
 * Sale del modo precisión para sliders.
 * @note
  Restaura el slider al rango original,
  elimina los estilos de precisión
  y ajusta el valor al entero más cercano
  (múltiplo de 10) para el modo normal.
 */

static void FineMode_Exit()
{
    if (!s_fineModeActive || s_fineSlider == nullptr) return;

    // Quitar estilos rojos
    lv_obj_remove_style(s_fineSlider, &s_styleFineInd,  LV_PART_INDICATOR);
    lv_obj_remove_style(s_fineSlider, &s_styleFineKnob, LV_PART_KNOB);

    // Restaurar rango original
    lv_slider_set_range(s_fineSlider, s_origMin, s_origMax);

    // Snap a entero (múltiplo de 10) para modo “grueso”
    int v = lv_slider_get_value(s_fineSlider);
    int snapped = ((v + 5) / 10) * 10;   // redondeo al entero más cercano (0.1*10)
    lv_slider_set_value(s_fineSlider, snapped, LV_ANIM_OFF);

    s_fineModeActive = false;
    s_fineSlider     = nullptr;
}

/**
 * @brief 
 * Entra en el modo precisión para sliders.
 * @note
  Ajusta el slider al modo de precisión,
  cambiando su rango a [N, N+1] en décimas
  y aplicando estilos rojos para indicar el modo.
  Si ya había otro slider en modo precisión,
  primero sale de ese modo. 
 * @param slider 
 */

static void FineMode_Enter(lv_obj_t* slider)
{
    if (slider == nullptr) return;

    FineStyle_InitOnce();

    // Si había otro slider en precisión, lo cerramos
    FineMode_Exit();

    s_fineSlider     = slider;
    s_fineModeActive = true;

    // Guardar rango original
    s_origMin = lv_slider_get_min_value(slider);
    s_origMax = lv_slider_get_max_value(slider);

    // Encerrar en ventana [N, N+1] usando décimas: [N*10, N*10+10]
    int v = lv_slider_get_value(slider);
    int base = (v / 10) * 10;
    lv_slider_set_range(slider, base, base + 10);

    // Aplicar estilos rojos
    lv_obj_add_style(slider, &s_styleFineInd,  LV_PART_INDICATOR);
    lv_obj_add_style(slider, &s_styleFineKnob, LV_PART_KNOB);
}

/**
 * @brief 
 * Configura la navegación para las distintas pantallas, sin incluir el salvapantallas
 * Screen1 - Scrren9.
 * @note
  Añade los objetos interactivos
  de la Screen1 al grupo de navegación
  y aplica el estilo de foco. 
  Fija el foco inicial en el primer botón. 
  (Repetido para cada pantalla con sus objetos) 
  @see SetupScreen2Nav
  @see SetupScreen3Nav
  @see SetupScreen4Nav 
 */

void SetupScreen1Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button1);
    lv_group_add_obj(g_navGroup, ui_Button3);
    lv_group_add_obj(g_navGroup, ui_Button4);
    lv_group_add_obj(g_navGroup, ui_Button18);
    lv_group_add_obj(g_navGroup, ui_ControlDeBrillo);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button1, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button3, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button4, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button18, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_ControlDeBrillo, &style_focus, LV_STATE_FOCUSED);

    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button1);
}

void SetupScreen2Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button7);
    lv_group_add_obj(g_navGroup, ui_Button8);
    lv_group_add_obj(g_navGroup, ui_Button2);
    lv_group_add_obj(g_navGroup, ui_MotorPrincipal);
    lv_group_add_obj(g_navGroup, ui_RotorDeCola);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button7, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button8, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button2, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_MotorPrincipal, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_RotorDeCola, &style_focus, LV_STATE_FOCUSED);

    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button7);
}

void SetupScreen3Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button5);
    lv_group_add_obj(g_navGroup, ui_Button9);
    lv_group_add_obj(g_navGroup, ui_Button11);
    lv_group_add_obj(g_navGroup, ui_Button10);
    lv_group_add_obj(g_navGroup, ui_Button12);
    lv_group_add_obj(g_navGroup, ui_Button6);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button5, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button9, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button11, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button10, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button12, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button6, &style_focus, LV_STATE_FOCUSED);

    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button9);
}

void SetupScreen4Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button14);
    lv_group_add_obj(g_navGroup, ui_Button25);
    lv_group_add_obj(g_navGroup, ui_Button21);
    lv_group_add_obj(g_navGroup, ui_Button13);
    lv_group_add_obj(g_navGroup, ui_SliderKpvv);
    lv_group_add_obj(g_navGroup, ui_SliderKpvh);
    lv_group_add_obj(g_navGroup, ui_SliderKivv);
    lv_group_add_obj(g_navGroup, ui_SliderKivh);
    lv_group_add_obj(g_navGroup, ui_SliderKdvv);
    lv_group_add_obj(g_navGroup, ui_SliderKdvh);
    lv_group_add_obj(g_navGroup, ui_SliderKphv);
    lv_group_add_obj(g_navGroup, ui_SliderKphh);
    lv_group_add_obj(g_navGroup, ui_SliderKihv);
    lv_group_add_obj(g_navGroup, ui_SliderKihh);
    lv_group_add_obj(g_navGroup, ui_SliderKdhv);
    lv_group_add_obj(g_navGroup, ui_SliderKdhh);
    lv_group_add_obj(g_navGroup, ui_SliderIsatvv);
    lv_group_add_obj(g_navGroup, ui_SliderIsatvh);
    lv_group_add_obj(g_navGroup, ui_SliderIsathv);
    lv_group_add_obj(g_navGroup, ui_SliderIsathh);
    lv_group_add_obj(g_navGroup, ui_SliderUvmax);
    lv_group_add_obj(g_navGroup, ui_SliderUhmax);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button14, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button25, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button21, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button13, &style_focus, LV_STATE_FOCUSED);

    lv_obj_add_style(ui_SliderKpvv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKpvh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKivv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKivh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKdvv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKdvh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKphv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKphh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKihv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKihh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKdhv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderKdhh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderIsatvv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderIsatvh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderIsathv, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderIsathh, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderUvmax, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SliderUhmax, &style_focus, LV_STATE_FOCUSED);

    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button14);
}

void SetupScreen5Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button15);
    lv_group_add_obj(g_navGroup, ui_BtnPIDZone);
    lv_group_add_obj(g_navGroup, ui_BtnTRMSZone);


    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button15, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_BtnPIDZone,   &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_BtnTRMSZone,  &style_focus, LV_STATE_FOCUSED);


    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button15);
}

void SetupScreen6Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Label64);
    lv_group_add_obj(g_navGroup, ui_Label65);
    lv_group_add_obj(g_navGroup, ui_Button26);
    lv_group_add_obj(g_navGroup, ui_Button16);
    lv_group_add_obj(g_navGroup, ui_Button17);


    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Label64, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Label65, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button26, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button16,  &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button17,  &style_focus, LV_STATE_FOCUSED);


    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Label64);
}

void SetupScreen7Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button20);

    lv_group_add_obj(g_navGroup, ui_Config1);
    lv_group_add_obj(g_navGroup, ui_Config2);
    lv_group_add_obj(g_navGroup, ui_Config3);
    lv_group_add_obj(g_navGroup, ui_Config4);
    lv_group_add_obj(g_navGroup, ui_SaveEEPROM);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button20, &style_focus, LV_STATE_FOCUSED);

    lv_obj_add_style(ui_Config1,   &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Config2,  &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Config3,   &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Config4,   &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_SaveEEPROM,   &style_focus, LV_STATE_FOCUSED);

    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button20);
}

void SetupScreen8Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button22);

    lv_group_add_obj(g_navGroup, ui_Label33);
    lv_group_add_obj(g_navGroup, ui_Label32);
    lv_group_add_obj(g_navGroup, ui_Label31);
    lv_group_add_obj(g_navGroup, ui_Label30);
    lv_group_add_obj(g_navGroup, ui_Label28);

    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button22, &style_focus, LV_STATE_FOCUSED);
    
    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button22);
}

void SetupScreen9Nav()
{
    lv_group_remove_all_objs(g_navGroup);
    lv_group_add_obj(g_navGroup, ui_Button19);
    lv_group_add_obj(g_navGroup, ui_Button23);


    // Aplicar estilo de foco a todos ellos
    lv_obj_add_style(ui_Button19, &style_focus, LV_STATE_FOCUSED);
    lv_obj_add_style(ui_Button23, &style_focus, LV_STATE_FOCUSED);
    
    // Fijar foco inicial en el primer botón (arriba a la izquierda)
    lv_group_focus_obj(ui_Button19);
}





