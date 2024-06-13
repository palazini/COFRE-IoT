//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                       _              //
//               _    _       _      _        _     _   _   _    _   _   _        _   _  _   _          //
//           |  | |  |_| |\| |_| |\ |_|   |\ |_|   |_| |_| | |  |   |_| |_| |\/| |_| |  |_| | |   /|    //    
//         |_|  |_|  |\  | | | | |/ | |   |/ | |   |   |\  |_|  |_| |\  | | |  | | | |_ | | |_|   _|_   //
//                                                                                       /              //
//////////////////////////////////////////////////////////////////////////////////////////////////////////


// Área de inclusão das bibliotecas
//-----------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ioplaca.h"   // Controles das Entradas e Saídas digitais e do teclado
#include "lcdvia595.h" // Controles do Display LCD
#include "hcf_adc.h"   // Controles do ADC
#include "MP_hcf.h"   // Controles do Motor
#include "connect.h"    // Controle do Wifi


#include "esp_netif.h"
#include "mqtt_client.h"
//#include "protocol_examples_common.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Área das macros
//-----------------------------------------------------------------------------------------------------------------------
# define PWD 1234

#define W_DEVICE_ID "666af2949ef4f6ca1472dcc1" //Use o DeviceID no Wegnology  
#define W_ACCESS_KEY "f884315b-aab8-43d9-b1e3-b2a05905e7f7" //use a chave de acesso e a senha
#define W_PASSWORD "4b20f97468a22781bab383c77e1c13e6aa11c0105acd0de7a491cbb3ae65dcb1"
#define W_TOPICO_PUBLICAR "wnology/666af2949ef4f6ca1472dcc1/state" //esse número no meio do tópico deve ser mudado pelo ID do seu device Wegnology
#define W_TOPICO_SUBSCREVER "wnology/666af2949ef4f6ca1472dcc1/command" // aqui também
#define W_BROKER "mqtt://broker.app.wnology.io:1883"
#define SSID "roteadorisa"
#define PASSWORD "senha1510"


#define DESLIGAR_TUDO       saidas&=0b00000000
#define LIGAR_RELE_1        saidas|0b00000001
#define DESLIGAR_RELE_1     saidas&=0b11111110
#define LIGAR_RELE_2        saidas|=0b00000010
#define DESLIGAR_RELE_2     saidas&=0b11111101
#define LIGAR_TRIAC_1       saidas|=0b00000100
#define DESLIGAR_TRIAC_1    saidas&=0b11111011
#define LIGAR_TRIAC_2       saidas|=0b00001000
#define DESLIGAR_TRIAC_2    saidas&=0b11110111
#define LIGAR_TBJ_1         saidas|=0b00010000
#define DESLIGAR_TBJ_1      saidas&=0b11101111
#define LIGAR_TBJ_2         saidas|=0b00100000
#define DESLIGAR_TBJ_2      saidas&=0b11011111
#define LIGAR_TBJ_3         saidas|=0b01000000
#define DESLIGAR_TBJ_3      saidas&=0b10111111
#define LIGAR_TBJ_4         saidas|=0b10000000
#define DESLIGAR_TBJ_4      saidas&=0b01111111
// Área de declaração de variáveis e protótipos de funções
//-----------------------------------------------------------------------------------------------------------------------

static const char *TAG = "Placa";
static uint8_t entradas, saidas = 0; //variáveis de controle de entradas e saídas
int controle = 0;
int senha = 0;
int tempo = 50;
int coluna = 0;
uint32_t adcvalor = 0;
char exibir [40];
char mensa [40];
const char *strVENT = "VENT\":"; 
const char *subtopico_temp = "{\"data\": {\"Temperatura\": ";
char * Inform;
bool ledstatus;
esp_mqtt_client_handle_t cliente;
int temperatura = 0;
int tensao = 0;
// Funções e ramos auxiliares para o cofre
//-----------------------------------------------------------------------------------------------------------------------
void abrir()
{
    int tentativas = 10;
    while(adcvalor <= 170 && tentativas > 0)
    {
        rotacionar_DRV(1,adcvalor<70?100:10,saidas);

        hcf_adc_ler(&adcvalor);                
        adcvalor = adcvalor*360/4095;
        sprintf(exibir,"%"PRIu32"  ", adcvalor);
        lcd595_write(1,0,exibir);
        entradas = io_le_escreve (saidas);
        tentativas--;
    }
}

void fechar()
{
    int tentativas = 10;
    while(adcvalor >= 40 && tentativas > 0)
    {
        rotacionar_DRV(0,adcvalor>140?100:10,saidas);
        hcf_adc_ler(&adcvalor);
        adcvalor = adcvalor*360/4095;
        sprintf(exibir,"%"PRIu32"  ", adcvalor);
        lcd595_write(1,0,exibir);
        entradas = io_le_escreve (saidas);
        tentativas--;
    }
}
void erro ()
{
    lcd595_clear();
    lcd595_write(1,0,"Nao autorizado");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
}

void restaura()
{
    senha = 0;
    lcd595_clear();
    lcd595_write(1,0, " Digite a senha ");
    lcd595_write(2,0, "[ ] [ ] [ ] [ ]");
    controle = 0;
    coluna = 0;
}

// Funções e ramos auxiliares para o IoT
//-----------------------------------------------------------------------------------------------------------------------
void mensagem(esp_mqtt_client_handle_t cliente) 
{
   if(strstr(Inform, strVENT))
        {
             if(strstr(Inform, "true"))
             {
                saidas = saidas | 0b00000001;
                printf ("LIGADO");
             }
             else
             {
                saidas = saidas & 0b11111110;
                printf("desligou");
             }
        }
        
       // gpio_set_level(TEC_SH_LD,ledstatus);//aqui é onde realmente acende ou apaga o LED embarcado
    
    }


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:        
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, W_TOPICO_PUBLICAR,mensa, 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        ESP_LOGI(TAG, "%s", mensa);       
        msg_id = esp_mqtt_client_subscribe(client, W_TOPICO_SUBSCREVER, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        Inform = event->data;
        mensagem(client); 
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = W_BROKER,
        .credentials.set_null_client_id = false,  
        .credentials.client_id = W_DEVICE_ID,
        .credentials.username = W_ACCESS_KEY,
        .credentials.authentication.password = W_PASSWORD,
        
    };
    cliente = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(cliente, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(cliente);
}

// Programa Principal
//-----------------------------------------------------------------------------------------------------------------------

void app_main(void)
{
    /////////////////////////////////////////////////////////////////////////////////////   Programa principal


    // a seguir, apenas informações de console, aquelas notas verdes no início da execução
    ESP_LOGI(TAG, "Iniciando...");
    ESP_LOGI(TAG, "Versão do IDF: %s", esp_get_idf_version());

    /////////////////////////////////////////////////////////////////////////////////////   Inicializações de periféricos (manter assim)
    ESP_ERROR_CHECK(nvs_flash_init());

    // inicializar os IOs e teclado da placa
    ioinit();      
    entradas = io_le_escreve(saidas); // Limpa as saídas e lê o estado das entradas

    // inicializar o display LCD 
    lcd595_init();
    lcd595_write(1,0,"Som. IoT - v1.0");
    vTaskDelay(1000 / portTICK_PERIOD_MS);  
  
    lcd595_write(1,0,"Inicializando   ");
    lcd595_write(2,0,"ADC             ");

    // Inicializar o componente de leitura de entrada analógica
    esp_err_t init_result = hcf_adc_iniciar();
    if (init_result != ESP_OK) {
        ESP_LOGE("MAIN", "Erro ao inicializar o componente ADC personalizado");
    }
    
    lcd595_write(2,0,"ADC / Wifi      ");
    lcd595_write(1,13,".  ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    // Inicializar a comunicação IoT
    wifi_init();    
    ESP_ERROR_CHECK(wifi_connect_sta(SSID, PASSWORD, 10000));

    lcd595_write(2,0,"C / Wifi / MQTT ");
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"Wifi / MQTT     ");
    lcd595_write(1,13,".. ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    sprintf(mensa,"%s %d }}",subtopico_temp,temperatura);
    mqtt_app_start();

    // inicializa driver de motor de passo com fins de curso nas entradas 6 e 7 da placa
    lcd595_write(2,0,"i / MQTT / DRV  ");
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"MQTT / DRV      ");
    lcd595_write(1,13,"...");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    DRV_init(6,7);

    // fecha a tampa, se estiver aberta
    hcf_adc_ler(&adcvalor);                
    adcvalor = adcvalor*360/4095;
    if(adcvalor>50)fechar();

    lcd595_write(2,0,"TT / DRV / APP  ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"DRV / APP       ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"V / APP         ");
    vTaskDelay(200 / portTICK_PERIOD_MS); 
    lcd595_write(2,0,"APP             ");
    //delay inicial para esperar a conexão com internet
    for(int i=0;i<10;i++)
    {
        lcd595_write(1,13,"   ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,".  ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,".. ");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
        lcd595_write(1,13,"...");
        vTaskDelay(200 / portTICK_PERIOD_MS); 
    }
    
    lcd595_clear();
    
    /////////////////////////////////////////////////////////////////////////////////////   Periféricos inicializados

    entradas = saidas;

    /////////////////////////////////////////////////////////////////////////////////////   Início do ramo principal                    
    while (1)                                                                                                                         
    {                                                                                                                                 
        //_______________________________________________________________________________________________________________________________________________________ //
        //-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - -  -  -  -  -  -  -  -  -  -  Escreva seu código aqui!!! //
             
        char tecla = le_teclado();
        hcf_adc_ler(&adcvalor);                
        adcvalor = adcvalor*360/4095;

      /*  if(// a definir, lampada)
        {
            

            esp_mqtt_client_publish(cliente, W_TOPICO_PUBLICAR, 
            "{\"data\": {\"TEMPERATURA\": \"30\" }}", 0, 0, 0);
        }
        else if(temperatura < 20)
        {
            
        } */
        
        hcf_adc_ler(&adcvalor);
        sprintf(&exibir[0], "%ld", adcvalor);
        lcd595_write(1,0, &exibir[0]);

        if(adcvalor > 700)
        {
            io_le_escreve(saidas);
            lcd595_write(2,0, "ligou vent");
        }
        else if(adcvalor < 500)
        {
            io_le_escreve(saidas);
            lcd595_write(2, 0, "desligou vent");
        }

        if (tecla >= '0' && tecla <= '9')
        {
            senha = senha * 10 + tecla - '0';
            controle++;
        }
        
        ESP_LOGI(TAG,"%"PRIu32"  ", adcvalor);

        //-  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  - -  -  -  -  -  -  -  -  -  -  Escreva seu só até aqui!!! //
        //________________________________________________________________________________________________________________________________________________________//
        vTaskDelay(200 / portTICK_PERIOD_MS);    // delay mínimo obrigatório, se retirar, pode causar reset do ESP
    }
    
    // caso erro no programa, desliga o módulo ADC
    hcf_adc_limpar();

    /////////////////////////////////////////////////////////////////////////////////////   Fim do ramo principal
    
}
