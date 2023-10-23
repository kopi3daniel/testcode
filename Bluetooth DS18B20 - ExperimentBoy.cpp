/*
 * Alimentation du module via 5V
 * PINs EN et STATE inutiles
 * PIN RX sur TX1 arduino (Mega)
 * PIN TX sur RX1 Arduino (Mega)
 * Pour utiliser avec un NANO, suivre le tuto dans la vidéo bonus: https://youtu.be/iC564i0wf0k
 */

 /*
  * Pour entrer en configuration du module, il faut l'alimenter en même temps qu'on appuie sur le bouton OU qu'on envoie 5V dans EN, même chose
  * Liste des commandes: https://www.teachmemicro.com/hc-05-bluetooth-command-list/
  * On envoie un code vide dans l'Arduino, et on relie le RX du HC au RX0 (celui du port usb) de l'arduino, TX même chose
  * Dans le moniteur série: 38400 baudrate, et Les deux, NL et RC
  * Envoyer "AT" signifie début de commande. On envoie AT jusqu'à ce qu'il renvoie "OK"
  * AT+NAME=<paramètre> change le nom visible du module (sans les <>)
  * AT+NAME=? affiche le nom
  * AT+PSWD=<code> Change le code pin de connexion
  * AT+PSWD? Affiche le code
  * AT+ROLE=<0,1>, 0= esclave, 1= master
  * AT+UART? Affiche le baudrate, le Stop bit, la Parité (défaut= 9600,0,0)
  * AT=UART=<param1>,<param2>,<param3> Configure ces variables, surtout pour changer le baudrate
  * AT+PAIR=<Param1>,<Param2>  Param1：Device Address Param2：Time out
  * AT+LINK=<Param> param = addresse, pour connecter à un appareil par son addresse
  */

  /*
   * Pour la sonde à température DS18B20:
   * Connecter le Rouge au +3.3 ou +5V, le noir au GND, le jaune sur un pin Data
   * IMPORTANT: Connecter une résistance de 4.7K entre le jaune et le rouge.
   */

#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>

SoftwareSerial SerialB(10, 11); // RX | TX Création d'un port série virtuel pour communiquer avec le module (OBLIGATOIRE sur arduino NANO quand on veut faire les commandes AT !!! On ne peut pas utiliser les pins RX et TX, sur Uno et Mega c'est ok)


#define pin_ds 2//Pin jaune de la sonde 
#define pin_relay 5
#define pin_button 7

#define temp_hysteresis 2 //degrés d'écart pour l'hystérésis du déclenchement de la pompe par température

//Création d'une instance oneWire pour communiquer avec la sonde
OneWire oneWire(pin_ds);

//On passe cette instance dans Daaaaaaallaaaaaaaas♪♫
DallasTemperature sensor(&oneWire);

float temp_water = 0.0, temp_water_up = 0.0, temp_water_down = 0.0;

const char mode_on = 'M', mode_off = 'm', mode_prog = 'P', mode_temp = 'T';
char mode = mode_off;

unsigned long t_start, t_measure = 0, measure_interval = 5000, prog_duration;


void setup() {
  SerialB.begin(38400);
  sensor.begin();
  
  pinMode(pin_relay,OUTPUT);
  pinMode(pin_button,INPUT_PULLUP);
  
  digitalWrite(pin_relay, 0);
  
}

void loop() {
  if(SerialB.available())
  {
    char mode_char = SerialB.read();
    switch (mode_char)
    {
        case mode_on:
        {
            mode = mode_on;
            break;
        }
        case mode_off:
        {
            mode = mode_off;
            break;
        }
        case mode_prog:
        {
            mode = mode_prog;
            t_start = millis();
            char buffer[SerialB.available()];
            String data="";
            while(SerialB.available())
                {
                    delay(1); // Pour ne pas lire plus vite que ce que les caractères arrivent
                    data+= char(SerialB.read()); //On récupère tous les catactères qui suivent le paramètre mode_prog, qui sera le nombre de minutes à activer la pompe
                }
            prog_duration = data.toInt() * 60000;
            break;
        }
        
        case mode_temp:
        {
            mode = mode_temp;
            char buffer[SerialB.available()];
            String data="";
            while(SerialB.available())
                {
                    delay(1);
                    data+= char(SerialB.read()); //On récupère tous les catactères qui suivent le paramètre mode_temp, qui sera la température seuil
                }
            temp_water_up = data.toInt();
            temp_water_down = temp_water_up - temp_hysteresis;
            break;
        }
    }
  }
    
    if(!digitalRead(pin_button))
    {
        //Si on appuie sur le bouton, on change l'état de la pompe et on force le passage au mode on ou off, on arrête le mode de programmation ou le mode température
        if(digitalRead(pin_relay)) mode = mode_off;
        
        else mode = mode_on;
        
        //digitalWrite(pin_relay,(!digitalRead(pin_relay)));
        
        while(!digitalRead(pin_button)){} //anti-rebond
        delay(50);
    }
    
    if (mode == mode_prog)
    {
        //Dans le cas où l'on vient de lancer une programmation pour x minutes, on lance la pompe et on vérifie
        if(millis()>t_start+prog_duration)
        {
            mode = mode_off;
            digitalWrite(pin_relay,0);
        }
        else
        digitalWrite(pin_relay,1);
    }
    
    
    else if (mode == mode_off) digitalWrite(pin_relay,0);
    
    else if(mode == mode_on) digitalWrite(pin_relay,1);
    
    if(millis() > t_measure + measure_interval)
    {
        //On lance un appel à lire la température tous les measure_interval, pour éviter l'envoi permanent étant donné que la lecture de la temp prend du temp -> cela retarderait la réactivité des autres fonctions
        t_measure = millis();
        sensor.requestTemperatures();
        temp_water = sensor.getTempCByIndex(0); //Ici, on lit à l'index 0 de sensor la valeur de la température (l'index change si on utilise plusieurs capteurs, mais alors là démerdez-vous, je sais pas faire, j'ai fait copier coller)
        
        String temp_water_message = "*t";
        temp_water_message += String(temp_water);
        
        SerialB.print(temp_water_message);   
            
        if(mode==mode_temp)
        {
            if(temp_water >= temp_water_up)
            {
                //On attend de descendre sous le seuil temp_water_down pour la couper. Cela crée une hystérésie, et évite qu'elle s'allume, éteigne, allume, éteigne...
                digitalWrite(pin_relay,1);
            }
            else if(temp_water <= temp_water_down)
                digitalWrite(pin_relay,0);
        } 
    }
}