# Debug class for testing LCD code without an actual LCD screen
import os
import sys
import termios
import fcntl


class VirtualLCD():

	def __init__(self):
		self.data = ""

		self.RED    = "red"
		self.GREEN  = "green"
		self.BLUE   = "blue"
		self.YELLOW = "yellow"
		self.TEAL   = "teal"
		self.VIOLET = "violet"
		self.WHITE  = "white"

		self.UP     = "i"
		self.DOWN   = "k"
		self.LEFT   = "j"
		self.RIGHT  = "l"
		self.SELECT = "m"

		self.lastButton = "";

		print("\x1b[H")
		print("                                                   \n")
		print("                                                   \n")
		print("                                                   \n")
		print("                                                   \n")
		print("                                                   \n")
		print("\x1b[H")
		return

	def getch(self):
		fd = sys.stdin.fileno()

		oldterm = termios.tcgetattr(fd)
		newattr = termios.tcgetattr(fd)
		newattr[3] = newattr[3] & ~termios.ICANON & ~termios.ECHO
		termios.tcsetattr(fd, termios.TCSANOW, newattr)

		oldflags = fcntl.fcntl(fd, fcntl.F_GETFL)
		fcntl.fcntl(fd, fcntl.F_SETFL, oldflags | os.O_NONBLOCK)

		try:
			while 1:
				try:
					c = sys.stdin.read(1)
					break
				except IOError: pass
		finally:
			termios.tcsetattr(fd, termios.TCSAFLUSH, oldterm)
			fcntl.fcntl(fd, fcntl.F_SETFL, oldflags)
		return c

	def noBlink(self):
		return

	def noCursor(self):
		return

	def clear(self):
		return

	def backlight(self,color):
		return

	def buttonPressed(self,button):
		# FIXME
		return 0
		if self.lastButton != "":
			if self.lastButton == button:
				self.lastButton = ""
				return 1
		else:
			ch = self.getch()
			if ch == button:
				return 1
			self.lastButton = ch
			
		return 0

	def home(self,):
		print("\x1b[H")
		return

	def message(self,message):
		self.data = message
		print(self.data)
		return

