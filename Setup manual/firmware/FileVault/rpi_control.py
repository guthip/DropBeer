import time
import serial
import socket
import re
import sys
from multiprocessing import Process

def showtime() :
	localtime = time.asctime( time.localtime(time.time()) )
	print "Local current time :", localtime

def toArduino() :

	def myip() :
		try :
			v =  ((([ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] if not ip.startswith("127.")] or [[(s.connect(("8.8.8.8", 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]]) + ["no IP found"])[0])
		except Exception, e:
			v = '<undef>'
		return v

	while ser.isOpen()  :
		ser.write('$MSG:ACT\r\n');
		ser.write('$CLR\r\n');
		ser.write('$PRN:Drop Beer\r\n')
		ser.write('$LOC:0,1\r\n')
		ser.write('$PRN:Beneteau FC 10\n\r' )
		time.sleep(5);
		ser.write('$MSG:ACT\r\n');
		ser.write('$CLR' + '\r\n');
		ser.write('$PRN:NMEA0183\r\n')
		ser.write('$LOC:0,1\r\n')
		ser.write('$PRN:10.10.10.1:10110\n\r' )
		time.sleep(5);
		ser.write('$MSG:ACT\r\n');
		ser.write('$CLR\r\n');
		ser.write('$PRN:VPN\r\n')
		ser.write('$LOC:0,1\r\n')
		ser.write('$PRN:10.10.10.1:5900\n\r' )
		time.sleep(5);
		ser.write('$MSG:ACT\r\n');
		ser.write('$CLR\r\n');
		ser.write('$PRN:VPN\r\n')
		ser.write('$LOC:0,1\r\n')
		ser.write('$PRN:10.10.10.1:5900\n\r' )
		time.sleep(5);



###########################################

def fromArduino():

	def readBuffer():
		try:
			data = ser.read(1)
			n = ser.inWaiting()
			if n:
				data = data + ser.read(n)
			return data
		except Exception, e:
			print "Read error, unexpected input: ", e
			sys.exit(1)

	def shutdown():
		ser.write('$LOC:0,1' + '\r\n')
		ser.write('$PRN:... indeed, Sir!\n\r' )
		showtime()
		print "Shutting down"
		command = "/usr/bin/sudo /sbin/shutdown now -P"
		import subprocess
		process = subprocess.Popen(command.split(), stdout=subprocess.PIPE)
		output = process.communicate()[0]


	newdata = ""
	data = ""
	line = ""

	while ser.isOpen() :

		if newdata:
			line = newdata
			newdata = ""

		line = line + readBuffer()

		try :
			if re.search("\r\n", line):
				data, newdata = line.split("\r\n")
		except ValueError :
			showtime()
			print('Read error from Arduino "{}"'.format(line))

		arduino_input = data.partition(":")

		if arduino_input[0] == "$AI0":
			print "VBat = " + arduino_input[2]
		
		if arduino_input[0] == "$STT":
			if arduino_input[2] == "STP":
				shutdown()

		line = ""


if __name__ == '__main__' :

	ser = serial.Serial( '/dev/ttyOP_ard', baudrate=9600, timeout=10)
	soc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	showtime()
	print "Starting up"

	p1 = Process(target=toArduino)
	p2 = Process(target=fromArduino)
	p1.start()
	p2.start()
	
#	ser.close()
