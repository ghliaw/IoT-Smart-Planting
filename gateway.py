#!/usr/bin/python

import threading
import _thread
import time
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
import serial
import time
import MySQLdb
import json
import sys

LoRa_comm = ">> radio_rx"
LoRa_data = ""

# 打开串口
ser = serial.Serial("/dev/ttyS0", 115200)

# Database
db = MySQLdb.connect(host="127.0.0.1", user="root", passwd="root", db="SmartPlanting")
conDB = db.cursor()

def setup():
    GPIO.setwarnings(False) # this ensures a clean exit
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(18,GPIO.OUT) 
    time.sleep(1)
    GPIO.output(18,GPIO.LOW)
    time.sleep(1)
    GPIO.output(18,GPIO.IN)
    ser.write('rf rx_con off'.encode())
    time.sleep(1)
    ser.write('rf set_sync 12'.encode())
    ser.readline()
    time.sleep(1)
    ser.write('rf set_freq 926500000'.encode())
    time.sleep(1)
    ser.write('rf set_sf 7'.encode())
    time.sleep(1)
    ser.write('rf set_bw 125'.encode())
    time.sleep(1)
    ser.write('rf rx_con on'.encode())
    time.sleep(1)
    print("Setup finish!")


def receive():
    while True:
        data  = ''
        while ser.in_waiting :
            data = ser.readline()
            data = data.decode()
          #  print(data)
            if(data.find(LoRa_comm) > 0 and data.find(LoRa_comm) < 20):
                if len(data) > 35:
                    print("error data")
                    break
                LoRa_data = data
                print (LoRa_data)
                print ("==")
                LoRa_data = LoRa_data.split()
                LoRa_data
                print(LoRa_data[2])
                #client.publish(Node2Pi, LoRa_data[2], qos=0, retain=False)
                print (LoRa_data[2][0:3])
                if(str(LoRa_data[2][0:3]) == '00f' ):
                    SensorNo = LoRa_data[2][3:6]
                    water =  LoRa_data[2][6:10] 
                    print("已新增水量資訊。感測點:" + SensorNo + ",water=" + water )
                    conDB.execute('INSERT INTO `wateryield`(`sensorNo`, `water`) VALUES (%s,%s)',(SensorNo,water))
                    db.commit()
                    LoRa_data = ""
                else :
                    if len(LoRa_data[2][0:6]) != 6 :
                        print("error data _line82")
                        break
                    if (str(LoRa_data[2][3:4]) != 'a' ): continue
                    SensorNo = LoRa_data[2][0:3]
                    Temp = LoRa_data[2][4:6]
                    Hum =  LoRa_data[2][6:8]
                    Soil =  LoRa_data[2][8:10]
                    light =  LoRa_data[2][10:12]
                    if (SensorNo is None or int(SensorNo) ==0 ): continue
                    elif (Temp is None or int(Temp) ==0 ): continue
                    elif (Hum is None or int(Hum) ==0 ): continue
                    elif (Soil is None or int(Soil) ==0 ): continue
                    else:
                        payload = {"temperature":Temp, "humidity":Hum, "soil":Soil, "light":light}
                        print (json.dumps(payload))
                        #要發布的主題和內容
                        client.publish("update/thingsboard", json.dumps(payload))
                        print("已新增環境數據。感測點:" + SensorNo + ",Temp=" + Temp + ",Hum=" + Hum + ",Soil=" + Soil)
                        conDB.execute('INSERT INTO `enviroment_info`(`sensorNo`, `temperature`, `humidity`, `soil_humi`) VALUES (%s,%s,%s,%s)',(SensorNo,Temp,Hum,Soil))
                        db.commit()
                        print("目前燈光狀態:" + light )
                        conDB.execute('UPDATE `light` SET status = %s WHERE ID = %s',(light,SensorNo))
                        db.commit()
                        LoRa_data = ""
                        conDB.execute("SELECT * FROM light where ID = " + SensorNo )
                        light_result = conDB.fetchall()
                        light_data = json.dumps([{"id": light_r[0], "status": light_r[2], "status_update": light_r[3]} for light_r in light_result])
                        light_data = json.loads(light_data)
                        conDB.execute("SELECT * FROM flowerpot where sensorID = " + SensorNo )
                        result = conDB.fetchall()
                        data = json.dumps([{"id": r[0], "mode": r[2]} for r in result])
                        data = json.loads(data)
                        if(int(data[0]['mode']) == 1):
                            print("Manual Mode")
                            conDB.execute("SELECT * FROM watering_manual where sensorNo = " + SensorNo )
                            result = conDB.fetchall()
                            data = json.dumps([{"id": r[0], "flow": r[2]} for r in result])
                            data = json.loads(data)
                            if(str(data) == '[]'):
                                print(" >> No Manual Watering INFO")
                                ser.write('rf rx_con off'.encode())
                                time.sleep(2.5)
                                lora_string = "rf tx " +str(SensorNo) +"F0000"+str(light_data[0]['status_update']).zfill(2)
                                ser.write(lora_string.encode())
                                #print(lora_string)
                                time.sleep(1)
                                ser.write('rf rx_con on'.encode())
                            else:
                                print(" >> Watering Flow : " , end='')
                                print(data[0]['flow'])
                                ser.write('rf rx_con off'.encode())
                                time.sleep(2.5)
                                lora_string = "rf tx " +str(SensorNo) +"F"+str(data[0]['flow']).zfill(4)+str(light_data[0]['status_update']).zfill(2)
                                ser.write(lora_string.encode())
                                print(lora_string)
                                time.sleep(1)
                                ser.write('rf rx_con on'.encode())
                                dataid = str(data[0]['id'])
                                conDB.execute("DELETE FROM `watering_manual` WHERE id = "+  dataid)
                                db.commit()
                        elif(int(data[0]['mode']) == 2):
                            print("Time Mode")
                            conDB.execute("SELECT * FROM watering_time where sensorID = "+ SensorNo +" ORDER BY time ASC")
                            result = conDB.fetchall()
                            data = json.dumps([{"id": r[0], "flow": r[3], "time": r[2]} for r in result])
                            data = json.loads(data)
                            if(str(data) == '[]'):
                                print(" >> No Time Watering INFO")
                                ser.write('rf rx_con off'.encode())
                                time.sleep(2.5)
                                lora_string = "rf tx " +str(SensorNo) +"F0000"+str(light_data[0]['status_update']).zfill(2)
                                ser.write(lora_string.encode())
                                print(lora_string)
                                time.sleep(1)
                                ser.write('rf rx_con on'.encode())
                            else:
                                now = time.time()
                                if(int(data[0]['time']) < int(now)):
                                    print(" >> Watering Flow : " , end='')
                                    print(data[0]['flow'])
                                    ser.write('rf rx_con off'.encode())
                                    time.sleep(2.5)
                                    lora_string = "rf tx " +str(SensorNo) +"F"+str(data[0]['flow']).zfill(4)+str(light_data[0]['status_update']).zfill(2)
                                    ser.write(lora_string.encode())
                                    time.sleep(1)
                                    ser.write('rf rx_con on'.encode())
                                    dataid = str(data[0]['id'])
                                    conDB.execute("DELETE FROM `watering_time` WHERE id = "+  dataid)
                                    db.commit()
                                else:
                                    print(" >> Time Watering Wating...")
                                    ser.write('rf rx_con off'.encode())
                                    time.sleep(2.5)
                                    lora_string = "rf tx " +str(SensorNo) +"F0000"+str(light_data[0]['status_update']).zfill(2)
                                    ser.write(lora_string.encode())
                                    print(lora_string)
                                    time.sleep(1)
                                    ser.write('rf rx_con on'.encode())
                        elif(int(data[0]['mode']) == 3):
                            print("Condition Mode")
                            conDB.execute("SELECT * FROM watering_condition where sensorID = " + SensorNo )
                            result = conDB.fetchall()
                            data = json.dumps([{"id": r[0], "cond": r[2] , "flow": r[3]} for r in result])
                            data = json.loads(data)
                            #print(data[0]['cond'])
                            #print(Soil)
                                
                            print (Soil)
                            print (data[0]['cond'])
                            if(int(Soil) == int(data[0]['cond'])):
                                print(" >> Watering Flow : " , end='')
                                print(data[0]['flow'])
                                ser.write('rf rx_con off'.encode())
                                time.sleep(2.5)
                                lora_string = "rf tx " +str(SensorNo) +"F"+str(data[0]['flow']).zfill(4)+str(light_data[0]['status_update']).zfill(2)
                                ser.write(lora_string.encode())
                                time.sleep(1)
                                print(lora_string)
                                ser.write('rf rx_con on'.encode())
                            else:
                                ser.write('rf rx_con off'.encode())
                                time.sleep(2.5)
                                lora_string = "rf tx " +str(SensorNo) +"F0000"+str(light_data[0]['status_update']).zfill(2)
                                ser.write(lora_string.encode())
                                print(lora_string)
                                time.sleep(1)
                                ser.write('rf rx_con on'.encode())
                        


Node2Pi = "Planting_receive"                     # MQTT topic for msg NodeMCU to PI3
Pi2Node = "Planting_send"                        # MQTT topic for msg PI3 to NodeMCU

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
#    client.subscribe(GIOT_ULTopic_prefix)
    client.subscribe("Planting_send")
    
# The callback for when a PUBLISH message is received from the server,
# PUBLISH Downlink message when message is received.
def on_message(client, userdata, msg):
    print("\n Subscribe message:")
    print(msg.topic + " " + str(msg.payload))


# Connect MQTT info
client = mqtt.Client(client_id="10503054a", protocol=mqtt.MQTTv31)
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set("", password="")
client.connect("127.0.0.1" ,port=1883, keepalive=60)                # MQTT IP address

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.

if __name__ == '__main__':
    setup()
  #  LoRa_receive =  receive()
    _thread.start_new_thread(receive,())
    client.loop_forever()

    while (True):
       pass