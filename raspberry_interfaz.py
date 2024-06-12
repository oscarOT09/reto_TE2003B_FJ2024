'''
  Reto: Sistema de Infoentretenimiento
  Código para interfaz en Raspberry
  Autores: Oscar Ortiz Torres A01769292 y Yonathan Romero Amador A01737244
  Diseño de sistemas en chip (TE2003B)
  Última fecha de moficación: 12/06/2024
'''

#Libreria para el sonido
import pygame

#Libreria para la interfaz
from PyQt5 import QtWidgets, QtCore, QtGui

#Libreria para leer la carpeta
import os

#Libreria para leer metadatos de .mp3
from mutagen.mp3 import MP3
from mutagen.id3 import ID3

#Libreria para acceder a Firebase
import firebase_admin
from firebase_admin import db, credentials

#Libreria para delays
import time

#Libreria para ejecucion simultanea
import threading

#Libreria de la OLED
from luma.core.interface.serial import i2c
from luma.oled.device import ssd1306
from PIL import Image, ImageDraw, ImageFont

#Libreria para control del sistema
import sys

#Credenciales para la Base de Datos
cred = credentials.Certificate("credentials.json")
firebase_admin.initialize_app(cred, {"databaseURL": "https://esp32-soc-default-rtdb.firebaseio.com"})

#Inicializa Pygame
pygame.mixer.init()

#Inicializa el puerto I2C
serial = i2c(port=1, address=0x3C)
device = ssd1306(serial)

#Crea una imagen en la OLED
width = device.width
height = device.height
image = Image.new('1', (width, height))
draw = ImageDraw.Draw(image)

#Funcion para la creacion de la lista de canciones
def leer_archivos_mp3(directorio):
    archivos = []
    for archivo in os.listdir(directorio):
        if archivo.endswith('.mp3'):
            archivos.append(os.path.join(directorio, archivo))
    return archivos

#Directorio de la carpeta
directorio_actual = os.path.dirname(os.path.abspath(__file__))
archivos_mp3 = leer_archivos_mp3(directorio_actual)

#Banderas y contadores del Reproductor
indice_actual = 0
posicion_actual = 0
reproduciendo = False
detencion_manual = False

#Se escribe PAUSA en la Base de Datos
db.reference("/music1/flag").set(0)

#Funcion del hilo. Monitoreo de Firebase
def monitoreo(player):
    while True:
        global num
        # Monitoreo del numero de cancion
        num = db.reference("/music1/num").get()
        if num != -1:
            if 1 <= num <= 100:
                player.actualizarNum()
        # Monitoreo del estado del reproductor
        flag = db.reference("/music1/flag").get()
        if flag == 2:
            player.siguiente_mp3()
            player.reproducir_mp3()
        elif flag == 3:
            player.anterior_mp3()
            player.reproducir_mp3()
        elif flag == 1:
            player.reproducir_mp3()
        elif flag == 0:
            player.detener_mp3()
        time.sleep(0.5)

#Clase Principal. La interfaz
class MP3Player(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.initUI()

    #Funcion que inicializa las partes de la interfaz.
    def initUI(self):
        self.setWindowTitle('Reproductor MP3')
        #Se crea el layout principal y se divide
        main_layout = QtWidgets.QHBoxLayout()
        #Se crea el layour de la parte Izquierda.
        music_layout = QtWidgets.QVBoxLayout()

        self.boton_inicio = QtWidgets.QPushButton('Inicio', self)
        self.boton_inicio.clicked.connect(self.iniciar_monitoreo)
        music_layout.addWidget(self.boton_inicio)

        self.label = QtWidgets.QLabel('Detenido', self)
        self.label.setFont(QtGui.QFont('Helvetica', 12))
        self.label.setAlignment(QtCore.Qt.AlignCenter)
        music_layout.addWidget(self.label)

        self.label2 = QtWidgets.QLabel('', self)
        self.label2.setFont(QtGui.QFont('Helvetica', 12))
        self.label2.setAlignment(QtCore.Qt.AlignCenter)
        music_layout.addWidget(self.label2)
        self.actualizar_OLED('Detenido', '')

        self.input_num = QtWidgets.QLineEdit()
        self.input_num.returnPressed.connect(self.procesar_input)
        music_layout.addWidget(QtWidgets.QLabel("Número de Canción: "))
        music_layout.addWidget(self.input_num)

        #Se crea el layout de la parte Derecha.
        control_layout = QtWidgets.QHBoxLayout()

        self.boton_anterior = QtWidgets.QPushButton(self)
        self.update_prev_button()
        self.boton_anterior.clicked.connect(self.anterior_mp3)
        control_layout.addWidget(self.boton_anterior)

        self.boton_reproducir = QtWidgets.QPushButton(self)
        self.update_play_button()
        self.boton_reproducir.clicked.connect(self.toggle_b)
        control_layout.addWidget(self.boton_reproducir)

        self.boton_siguiente = QtWidgets.QPushButton(self)
        self.update_next_button()
        self.boton_siguiente.clicked.connect(self.siguiente_mp3)
        control_layout.addWidget(self.boton_siguiente)

        #Imagen del Mbot.
        music_layout.addLayout(control_layout)
        mbot_layout = QtWidgets.QVBoxLayout()
        self.image_label = QtWidgets.QLabel(self)
        self.image_label.setPixmap(QtGui.QPixmap("image.png").scaled(200, 200, QtCore.Qt.KeepAspectRatio))
        self.image_label.setAlignment(QtCore.Qt.AlignCenter)
        mbot_layout.addWidget(self.image_label)

        mbot_buttons_layout = QtWidgets.QGridLayout()

        self.boton_girar_l = QtWidgets.QPushButton(self)
        self.update_girar_l_button()
        self.boton_girar_l.clicked.connect(self.girar_l)
        mbot_buttons_layout.addWidget(self.boton_girar_l, 0, 0)

        self.boton_forward = QtWidgets.QPushButton(self)
        self.update_forward_button()
        self.boton_forward.clicked.connect(self.mbot_forward)
        mbot_buttons_layout.addWidget(self.boton_forward, 0, 1)

        self.boton_girar_r = QtWidgets.QPushButton(self)
        self.update_girar_r_button()
        self.boton_girar_r.clicked.connect(self.girar_r)
        mbot_buttons_layout.addWidget(self.boton_girar_r, 0, 2)

        self.boton_left = QtWidgets.QPushButton(self)
        self.update_left_button()
        self.boton_left.clicked.connect(self.mbot_left)
        mbot_buttons_layout.addWidget(self.boton_left, 1, 0)

        self.boton_dance = QtWidgets.QPushButton(self)
        self.update_dance_button()
        self.boton_dance.clicked.connect(self.dance_Floor)
        mbot_buttons_layout.addWidget(self.boton_dance, 1, 1)

        self.boton_right = QtWidgets.QPushButton(self)
        self.update_right_button()
        self.boton_right.clicked.connect(self.mbot_right)
        mbot_buttons_layout.addWidget(self.boton_right, 1, 2)

        self.boton_backward = QtWidgets.QPushButton(self)
        self.update_backward_button()
        self.boton_backward.clicked.connect(self.mbot_backward)
        mbot_buttons_layout.addWidget(self.boton_backward, 2, 1)

        #Se imprimen cada Layout y todos sus Sub-layouts
        mbot_layout.addLayout(mbot_buttons_layout)
        main_layout.addLayout(music_layout)
        main_layout.addLayout(mbot_layout)
        self.setLayout(main_layout)
        self.show()

    #Funcion del Input. Leer entrada
    def procesar_input(self):
        inp = self.input_num.text()
        try:
            valor = int(inp)
            if 1 <= valor <= 100:
                self.actualizarvalor(valor)
        except ValueError:
            print("Ingrese Numero")
            
    #Funcion del Input. Actualizar Indice de Cancion
    def actualizarvalor(self,valor):
        global indice_actual
        db.reference("/music1/num").set(-1)
        self.detener_mp3()
        indice_actual = valor-1
        self.reproducir_mp3()

    #Imagen del boton Reproducir/Detener        
    def update_play_button(self):
        icon = QtGui.QIcon()
        if reproduciendo:
            icon.addPixmap(QtGui.QPixmap("play.png"))
        else:
            icon.addPixmap(QtGui.QPixmap("stop.png"))
        self.boton_reproducir.setIcon(icon)

    #Imagen del boton Anterior
    def update_prev_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("back.png"))
        self.boton_anterior.setIcon(icon)
        
    #Imagen del boton Siguiente
    def update_next_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("next.png"))
        self.boton_siguiente.setIcon(icon)

    #Imagen del boton Bailar
    def update_dance_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("dance.png"))
        self.boton_dance.setIcon(icon)

    #Imagen del boton Adelante
    def update_forward_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("f.png"))
        self.boton_forward.setIcon(icon)

    #Imagen del boton Atras
    def update_backward_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("b.png"))
        self.boton_backward.setIcon(icon)

    #Imagen del boton Izquierda
    def update_left_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("l.png"))
        self.boton_left.setIcon(icon)

    #Imagen del boton Siguiente
    def update_right_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("r.png"))
        self.boton_right.setIcon(icon)

    #Imagen del boton Girar Izquierda
    def update_girar_l_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("zl.png"))
        self.boton_girar_l.setIcon(icon)

    #Imagen del boton Girar Derecha
    def update_girar_r_button(self):
        icon = QtGui.QIcon(QtGui.QPixmap("zr.png"))
        self.boton_girar_r.setIcon(icon)

    #Funcion del Boton Inicio, crea el Hilo.
    def iniciar_monitoreo(self):
        self.thread = threading.Thread(target=monitoreo, args=(self,))
        self.thread.daemon = True
        self.thread.start()
        self.boton_inicio.setDisabled(True)

    #Funcion que reproduce musica
    def reproducir_mp3(self):
        global reproduciendo, indice_actual, posicion_actual
        if not reproduciendo:
            #Carga la lista, el indice y la posicion de las canciones
            archivo = archivos_mp3[indice_actual]
            pygame.mixer.music.load(archivo)
            pygame.mixer.music.play(start=posicion_actual)

            #Se obtienen los metadatos de la cancion 
            audio = MP3(archivo, ID3=ID3)
            titulo = audio.get('TIT2', 'TÃ­tulo desconocido')
            artista = audio.get('TPE1', 'Artista desconocido')

            #Se actualizan los labels y se manda a imprimir al OLED
            nombre_archivo = f"{titulo}"
            n_artista = f"{artista}"
            self.label.setText(f'Reproduciendo: {nombre_archivo}')
            self.label2.setText(f'Artista: {artista} - Numero: {indice_actual+1}')
            self.actualizar_OLED(nombre_archivo, n_artista)

            #Se actualizan las banderas locales y de la base de datos
            reproduciendo = True
            detencion_manual = False
            db.reference("/music1/flag").set(1)

            #Se actualiza el boton
            self.update_play_button()
            time.sleep(0.1)

    #Funcion para detener la musica
    def detener_mp3(self):
        global posicion_actual, reproduciendo
        if reproduciendo:
            #Se guarda la posicion de la cancion y la detiene
            posicion_actual = pygame.mixer.music.get_pos() / 1000.0
            pygame.mixer.music.stop()

            #Se actualizan los labels y la OLED
            self.label.setText('Detenido')
            self.label2.setText('')
            self.actualizar_OLED('Detenido', '')

            #Se actualizan las banderas locales y de la base de datos
            db.reference("/music1/flag").set(0)
            reproduciendo = False
            detencion_manual = True

            #Se actualiza el boton
            self.update_play_button()
            time.sleep(0.1)

    #Funcion del boton Reproducir/Detener
    def toggle_b(self):
        #Cambia entre las 2 funciones que hace el boton con un toggle.
        if reproduciendo:
            self.detener_mp3()
        else:
            self.reproducir_mp3()

    #Funcion del boton Siguiente
    def siguiente_mp3(self):
        global indice_actual, posicion_actual
        #Detiene la cancion
        self.detener_mp3()
        
        #Se actualiza el indice de la lista de canciones
        indice_actual = (indice_actual + 1) % len(archivos_mp3)
        
        #Se reinicia la posicion
        posicion_actual = 0
        time.sleep(0.1)
        
        #Se reproduce la nueva cancion
        self.reproducir_mp3()

    #Funcion del boton Anterior
    def anterior_mp3(self):
        global indice_actual, posicion_actual
        #Detiene la cancion
        self.detener_mp3()
        
        #Se actualiza el indice de la lista de canciones
        indice_actual = (indice_actual - 1 + len(archivos_mp3)) % len(archivos_mp3)

        #Se reinicia la posicion
        posicion_actual = 0
        time.sleep(0.1)

        #Se reproduce la nueva cancion
        self.reproducir_mp3()

    #Funcion que lee actualiza el indice de cancion desde la base de datos
    def actualizarNum(self):
        global indice_actual
        self.detener_mp3()
        db.reference("/music1/num").set(-1)
        self.detener_mp3()
        indice_actual = num-1
        self.reproducir_mp3()

    #Funcion del Boton Dance, manda un 7.
    def dance_Floor(self):
        db.reference("/mbot1/flag").set(7)

    #Funcion del Boton Adelante, manda un 1.
    def mbot_forward(self):
        db.reference("/mbot1/flag").set(1)

    #Funcion del Boton Atras, manda un 2.
    def mbot_backward(self):
        db.reference("/mbot1/flag").set(2)

    #Funcion del Boton Izquierda, manda un 3.
    def mbot_left(self):
        db.reference("/mbot1/flag").set(3)

    #Funcion del Boton Derecha, manda un 4.
    def mbot_right(self):
        db.reference("/mbot1/flag").set(4)

    #Funcion del Boton Girar Izquierda, manda un 5.
    def girar_l(self):
        db.reference("/mbot1/flag").set(5)

    #Funcion del Boton Girar Derecha, manda un 6.
    def girar_r(self):
        db.reference("/mbot1/flag").set(6)

    #Funcion para imprimir en la OLED
    def actualizar_OLED(self, titulo, autor):
        #Se vacia la OLED
        draw.rectangle((0, 0, width, height), outline=0, fill=0)

        #Se revisa si el nombre de la cancion entra en una fila de la OLED
        titulo1, titulo2 = self.dividirTitulo(titulo);

        #Se declaran las variables del Texto
        font = ImageFont.load_default()
        padding = 5
        top = padding
        x = padding
        line_height = 15

        #Se imprimen cada linea
        draw.text((x, top), autor, font=font, fill=255)
        top += line_height
        draw.text((x, top), titulo1, font=font, fill=255)
        top += line_height
        draw.text((x, top), titulo2, font=font, fill=255)

        #Se imprime la pantalla
        device.display(image)

    #Funcion para revisar el tamaño del string
    def dividirTitulo(selg,titulo):
        #La OLED entran 20 char en una fila.
        if len(titulo) > 20:
            #Se divide en 2 strings
            titulo1 = titulo[:20]
            titulo2 = titulo[20:]
            return titulo1, titulo2
        else:
            #No se divide
            return titulo, ""

#Se iniciaaliza la Clase MP3Player
if __name__ == '__main__':
    app = QtWidgets.QApplication([])
    player = MP3Player()
    app.exec_()
