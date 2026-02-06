/*
  Incubator LispLibrary - Version 1.0 - Sep 2025
  Hartmut Grawe - github.com/ersatzmoco - Sep 2025

  Licensed under the MIT license: https://opensource.org/licenses/MIT
*/


const char LispLibrary[] PROGMEM =  R"lisplibrary(

#|fan mode object (closure)|#
(defun fan-mode (pin)

	#|define additional encapsulated attributes and methods to be called internally|#
	(let* 	((status "slow"))

		#|access attributes and methods via "messages"|#
		(lambda (&optional msg)
			(case msg
				#|controlled access of some encapsulated attributes|#
				(pin pin)
				(status status)

				#|init: activate relay pin and switch relay to "slow" voltage ("high" state is relay off, "low" activated)|#
				(init (pinmode pin t) (digitalwrite pin t))

				(slow (digitalwrite pin t) (setf status "slow"))
				(fast (digitalwrite pin nil) (setf status "fast"))

				#|default (no message): object introspection as list|#
				(t (list pin status))
			)
		)
	)
)



#|fan object (closure)|#
(defun fan (pin)

	#|define additional encapsulated attributes and methods to be called internally|#
	(let* 	((status nil))

		#|access attributes and methods via "messages"|#
		(lambda (&optional msg)
			(case msg
				#|controlled access of some encapsulated attributes|#
				(pin pin)
				(status status)

				#|methods|#
				#|init: activate relay pin and switch relay off ("high" state is off, "low" activated)|#
				(init (pinmode pin t) (digitalwrite pin t))

				(on (digitalwrite pin nil) (setf status t))
				(off (digitalwrite pin t) (setf status nil))

				#|default (no message): object introspection as list|#
				(t (list pin status))
			)
		)
	)
)


#|hatch object (closure)|#
(defun hatch (rpin spin snum o-angle c-angle)

	#|define additional encapsulated attributes and methods to be called internally|#
	(let* 	((status nil))

		#|access attributes and methods via "messages"|#
		(lambda (&optional msg)
			(case msg
				#|controlled access of some encapsulated attributes|#
				(rpin rpin)
				(spin spin)
				(snum snum)
				(status status)

				#|methods|#
				#|init: activate relay pin and switch relay off ("high" state is off, "low" activated)|#
				#|attach servo number snum to pin spin|#
				(init (pinmode rpin t) (digitalwrite rpin t) (servo-attach snum spin))

				(open (servo-write snum o-angle) (digitalwrite rpin nil) (delay 500) (digitalwrite rpin t) (setf status t))
				(close (servo-write snum c-angle) (digitalwrite rpin nil) (delay 500) (digitalwrite rpin t) (setf status nil))

				#|default (no message): object introspection as list|#
				(t (list rpin spin snum status))
			)
		)
	)
)


#|cooler object (closure)|#
(defun cooler (target-temp chatch cfan fanmode)

	#|define additional encapsulated attributes and methods to be called internally|#
	(let* 	((is-active nil)

			#|on: switch cooler on and set status active|#
			(on (lambda ()
					(chatch 'open)
					(setf is-active t)
					(fanmode 'fast) (cfan 'on)
				)
			)

			#|off: switch cooler off and set status inactive|#
			(off (lambda ()
					(chatch 'close)
					(setf is-active nil)
					(cfan 'off)
				)
			))

		#|access attributes and methods via "messages"|#
		(lambda (&optional msg)
			(case msg
				#|controlled access of some encapsulated attributes|#
				(target-temp target-temp)
				(is-active is-active)

				#|methods|#
				#|init not necessary unless additional special cooling element is used|#

				#|check: control status and switch cooling elements off if temperature equal or lower target-temp|#
				(check
					(when is-active
						(when (<= (bme-read-temp) target-temp) (off))
					)
				)
				(on (on))
				(off (off))

				#|default (no message): object introspection|#
				(t target-temp)
			)
		)
	)
)


#|heater object (closure)|#
(defun heater (sec-on sec-off target-temp pin hfan fanmode)

	#|define additional encapsulated attributes and methods to be called internally|#
	(let* 	((start-time 0)
			(is-heating nil)
			(is-waiting nil)
			(is-active nil)

			#|off: switch heater off and set status inactive|#
			(off (lambda ()
					(digitalwrite pin t) #|relay off |#
					(setf is-active nil) (setf is-heating nil) (setf is-waiting nil)
					(hfan 'off)
				)
			)
			#|heat: switch to heat mode |#
			(heat (lambda ()
					(setf start-time (millis)) 
					(setf is-heating t) (setf is-waiting nil)
					(digitalwrite pin nil) #|relay on|#
				)
			)
			#|wait: switch to wait mode |#
			(wait (lambda ()
					(setf start-time (millis)) 
					(setf is-heating nil) (setf is-waiting t) 
					(digitalwrite pin t) #|relay off|#
				)
			))

		#|access attributes and methods via "messages"|#
		(lambda (&optional msg)
			(case msg
				#|controlled access of some encapsulated attributes|#
				(sec-on sec-on)
				(sec-off sec-off)
				(target-temp target-temp)
				(pin pin)

				(is-heating is-heating)
				(is-waiting is-waiting)
				(is-active is-active)

				#|methods|#
				#|init: activate relay pin and switch relay off ("high" state is off, "low" activated)|#
				(init (pinmode pin t) (digitalwrite pin t))

				#|on: start interval heating process - attention: status must be polled!|#
				(on 
					(setf is-active t)
					(fanmode 'slow)
					(hfan 'on)
					(heat)
				)

				#|off: call internal 'off' method from outside|#
				(off (off))

				#|check: control status and switch heat elements off if sec-on is exceeded|#
				(check
					(when is-active
						(when (>= (bme-read-temp) target-temp) (off))
						(when is-heating 
							(when (> (abs (- (millis) start-time)) (* sec-on 1000))
								(wait)
							)
						)
						(when is-waiting
							(when (> (abs (- (millis) start-time)) (* sec-off 1000))
								(heat)
							)
						)
					)
				)

				#|default (no message): object introspection as list|#
				(t (list sec-on sec-off target-temp pin))
			)
		)
	)
)

(defun setup ()
	#|data logging on/off|#
	(defvar logging nil)

	#|minimum temperature 28 degrees celsius|#
	(defvar mintemp 28)
	#|maximum temperature 32 degrees celsius|#
	(defvar maxtemp 32)
	#|target temperature 30 degrees celsius|#
	(defvar targettemp 30)
	#|check temperature every ? milliseconds|#
	(defvar polltime 5000)
	#|reset timestamp|#
	(defvar timestamp (millis))
	(defvar now 0)

	#|initialize sensor and oled|#
	(bme-begin)
	(oled-begin)
	(oled-set-font 1)
	(oled-write-string 0 16 (format nil "TMP: ~2a C" (floor (bme-read-temp))))
	(oled-write-string 0 37 (format nil "HUM: ~3a%" (floor (bme-read-hum))))

	#|initialize hardware|#
	(defvar fanmode (fan-mode 0))
	(fanmode 'init)
	(defvar myfan (fan 1))
	(myfan 'init)
	(defvar myhatch (hatch 2 28 1 0 95))
	(myhatch 'init)
	(myhatch 'close)
	(defvar myheater (heater 30 30 targettemp 3 myfan fanmode))
	(myheater 'init)
	(defvar mycooler (cooler targettemp myhatch myfan fanmode))

	(oled-set-font 0)
	(oled-write-string 0 49 "HEATER off")
	(oled-write-string 0 62 "COOLING off")

	#|create empty log file|#
	(when logging
		(with-sd-card (str "log.csv" 2) (princ "TMP,HUM,Heater,Cooler" str) (terpri str))
	)
)


#|main loop|#
(defun mainloop ()

	(loop
		#|check conditions in defined intervals|#
		(setf now (millis))
		(when (> (abs (- now timestamp)) polltime)
			(setf timestamp now)

			#|check temperature conditions and react|#
			(if (myheater 'is-active)
				(myheater 'check)
				(when (< (bme-read-temp) mintemp) (when (mycooler 'is-active) (mycooler 'off)) (myheater 'on))
			)
			(if (mycooler 'is-active)
				(mycooler 'check)
				(when (> (bme-read-temp) maxtemp) (myheater 'off) (mycooler 'on))
			)

			#|display status|#
			(oled-set-font 1)
			(oled-write-string 0 16 (format nil "TMP: ~2a C" (floor (bme-read-temp))))
			(oled-write-string 0 37 (format nil "HUM: ~3a%" (floor (bme-read-hum))))

			(oled-set-font 0)
			(if (myheater 'is-active)
				(progn 
					(oled-write-string 0 49 "HEATER")
					(oled-set-color 0) (oled-fill-rect 54 41 50 10) (oled-set-color 1)
				)
				(oled-write-string 0 49 "HEATER off")
			)
			(if (mycooler 'is-active)
				(progn 
					(oled-write-string 0 62 "COOLING") 
					(oled-set-color 0) (oled-fill-rect 62 54 50 10) (oled-set-color 1)
				)
				(oled-write-string 0 62 "COOLING off")
			)

			(when logging
				(with-sd-card (str "log.csv" 1)
					(princ (bme-read-temp) str) (princ "," str)
					(princ (bme-read-hum) str) (princ "," str)
					(if (myheater 'is-active)
						(if (myheater 'is-heating)
							(progn (princ 10 str) (princ "," str))
							(progn (princ 5 str) (princ "," str))
						)
						(progn (princ 0 str) (princ "," str))
					)
					(if (mycooler 'is-active)
						(princ 15 str)
						(princ 0 str)
					)
					(terpri str)
				)
			)
		)
	)
)

#|direct startup after reset|#
(setup)
(mainloop)

)lisplibrary";