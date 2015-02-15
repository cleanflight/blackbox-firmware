all: openlog_blackbox.zip

openlog_blackbox.zip : Readme.md OpenLog_v3_Blackbox/OpenLog_v3_Blackbox.ino OpenLog_v3_Blackbox/OpenLog_v3_Blackbox.cpp.hex
	zip $@ $^ -r libs/

clean:
	rm openlog_blackbox.zip