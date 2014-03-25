#To (re)compile C bytecode:
#
#/path/to/mruby/bin/mrbc -Bblinker -oblinker.c blinker.rb
#

class Blinker
	include Arduino
	attr_accessor :interval ,:pin
	
	def initialize(pin, interval_ms)
		Serial2.println("new Blinker:pin=#{pin}, interval=#{interval_ms}[ms]")
		@pin = pin
		@interval = interval_ms
	end

	def blink_once
		#Serial2.println("start LED:#{@pin}")
		digitalWrite(@pin, HIGH)
		#Serial2.println("P1:#{@pin}")
		#Arduino.delay(300)
		#FreeRTOS.sleep(@interval)
		FreeRTOS.sleep(400)
		#Serial2.println("P2:#{@pin}")
		digitalWrite(@pin, LOW)
		#Serial2.println("P3:#{@pin}")
		#FreeRTOS.sleep(@interval)
		#Arduino.delay(100)
		FreeRTOS.sleep(1000)
		#Serial2.println("end LED:#{@pin}")
		nil
	end
end