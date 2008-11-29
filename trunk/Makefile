DAT=../../dat
TARGET=arm-apple-darwin9
CC=$(DAT)/pre/bin/arm-apple-darwin9-gcc
LD=$(CC)
#CFLAGS=-x objective-c -DMOBILE -DVer2 -I./ -I./UIKit -I./Foundation -I./CoreFoundation -I/var/include -I/usr/include/gcc/darwin/4.2
CFLAGS= -I$(DAT)/pre/include/ \
        -I$(DAT)/pre/include/mach-o \
        -I$(DAT)/sys/usr/lib/gcc/arm-apple-darwin9/4.2.1/include \
        -I$(DAT)/sys/usr/include/gcc/darwin/4.2 \
        -I$(DAT)/sys/usr/include \
        -I.
#CFLAGS=-O
LDFLAGS=-lobjc \
	-framework CoreFoundation \
	-framework Foundation \
	-framework UIKit \
	-framework CoreGraphics \
	-framework GraphicsServices \
	-framework CoreSurface \
	-framework CoreAudio \
	-framework IOKit \
	-framework AudioToolbox \
	-L"$(DAT)/sys/usr/lib" \
        -F"." \
	-F"$(DAT)/sys/System/Library/Frameworks" \
	-F"$(DAT)/sys/System/Library/PrivateFrameworks" \
	-bind_at_load \
	-L/usr/lib/ -lgcc_s.1 -lstdc++.6 -lbz2

APPNAME=MobileWiki
FILES=mainapp.o Application.o HistListView.o LangListView.o srvmain.o\
	CPPStringUtils.oo ImageIndex.oo  StopWatch.oo TitleIndex.oo   WikiMarkupGetter.oo\
	ConfigFile.oo Settings.oo StringUtils.oo  WikiArticle.oo  WikiMarkupParser.oo

        
#all:    $(APPNAME) package
all:    $(APPNAME) 

$(APPNAME):  $(FILES)

	$(LD) $(LDFLAGS) -v -o $@ $^
	#/usr/bin/ldid -S $(APPNAME)

%.o:    %.m
		$(CC) -c $(CFLAGS) -x objective-c++ $(CPPFLAGS) $< -o $@

%.oo:	%.cpp
		$(CC) -c $(CFLAGS) -x c++ $(CPPFLAGS) $< -o $@

clean:
	rm -rf *.o *.oo *~ $(APPNAME) $(APPNAME).app

package: $(APPNAME)
	rm -fr $(APPNAME).app
	mkdir -p $(APPNAME).app
	cp $(APPNAME) $(APPNAME).app/$(APPNAME)
	cp Info.plist $(APPNAME).app/Info.plist
	cp *.png $(APPNAME).app/
	cp -r daemon $(APPNAME).app/

debug: $(APPNAME) package

