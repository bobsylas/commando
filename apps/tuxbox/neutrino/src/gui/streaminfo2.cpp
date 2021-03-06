/*
	$Id: streaminfo2.cpp,v 1.50 2012/08/14 18:25:25 rhabarber1848 Exp $
	
	Neutrino-GUI  -   DBoxII-Project

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

//
// -- this module is a evil hack
// -- Neutrino lacks a proper OSD class
//


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define SCREEN_X	720
#define SCREEN_Y	572

#include <iostream>

#include <gui/streaminfo2.h>
#include <gui/imageinfo.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/screen_max.h>
#include <gui/color.h>
#include <gui/widget/icons.h>
#include <gui/widget/buttons.h>

#include <daemonc/remotecontrol.h>
extern CRemoteControl * g_RemoteControl; /* neutrino.cpp */

CStreamInfo2::CStreamInfo2()
{
	pig = new CPIG (0);
	frameBuffer = CFrameBuffer::getInstance();

	font_head = SNeutrinoSettings::FONT_TYPE_MENU_TITLE;
	font_info = SNeutrinoSettings::FONT_TYPE_MENU;
	font_small = SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL;

	hheight     = g_Font[font_head]->getHeight();
	iheight     = g_Font[font_info]->getHeight();
	sheight     = g_Font[font_small]->getHeight();

	width	= w_max(SCREEN_X, 20);
	height	= h_max(SCREEN_Y, 20);

	x	= getScreenStartX(width); //mainwindow position
	y	= getScreenStartY(height);
	endX	= x + width;
	endY	= y + height;

	max_height = SCREEN_Y-1;
	max_width  = SCREEN_X-1;
	
	old_x = 0;
	old_y = 0;

	sigBox_pos = 0;
	paint_mode = 0;
	
	signal.max_sig = 0;
	signal.max_snr = 0;
	signal.max_ber = 0;

	signal.min_sig = 100000;
	signal.min_snr = 100000;
	signal.min_ber = 100000;

	rate.short_average = 0;
	rate.max_short_average = 0;
	rate.min_short_average = 20000;
	
	brc = 0;
	int mode = CNeutrinoApp::getInstance()->getMode();

#ifdef ENABLE_RADIOTEXT
	/* stop Radiotext if in Radiomode */
	if (mode == NeutrinoMessages::mode_radio && g_settings.radiotext_enable)
	{
		delete g_Radiotext;
		g_Radiotext = NULL;
	}
#endif
 
	if (!g_Zapit->isRecordModeActive())
		if (mode == NeutrinoMessages::mode_tv) { 
			current_apid = -1;		
#ifdef HAVE_DBOX_HARDWARE
			actmode = g_Zapit->PlaybackState();
			if (actmode == 0) { //PES Mode aktiv
#endif
				CZapitClient::responseGetPIDs allpids;
				g_Zapit->getPIDS(allpids);
				for (unsigned int i = 0; i < allpids.APIDs.size(); i++) {
					if (allpids.APIDs[i].is_ac3) { //Suche Ac3 Pid
						if (i == allpids.PIDs.selected_apid) { //Aktuelle Pid ist ac3 pid
							current_apid = allpids.PIDs.selected_apid; //Speichere aktuelle pid und switche auf Stereo
							g_Zapit->setAudioChannel(0);
							break;
						}
					}
				}	
#ifdef HAVE_DBOX_HARDWARE
				g_Zapit->PlaybackSPTS();
			}
#endif
		
			if ( g_RemoteControl->current_PIDs.PIDs.vpid != 0 ) {
				brc = new BitrateCalculator(g_RemoteControl->current_PIDs.PIDs.vpid);
			} else if (!g_RemoteControl->current_PIDs.APIDs.empty()) {
				brc = new BitrateCalculatorRadio(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
			}
		} else {
			if (!g_RemoteControl->current_PIDs.APIDs.empty()){
				brc = new BitrateCalculatorRadio(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
			}
		}
}

CStreamInfo2::~CStreamInfo2()
{
#ifdef HAVE_DBOX_HARDWARE
	if (!g_Zapit->isRecordModeActive()) {
		if (actmode == 0) {
			g_Zapit->PlaybackPES();
			if (current_apid != -1) {
				g_Zapit->setAudioChannel(current_apid);		
			}
		}
	}
#endif

	delete pig;
	if (brc) delete brc;
		
#ifdef ENABLE_RADIOTEXT
	/* restart Radiotext if in Radiomode and enabled */
	if ((CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio) && g_settings.radiotext_enable)
	{
		g_Radiotext = new CRadioText;
		g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
	}
#endif
}

int CStreamInfo2::exec()
{
	int res;
	paint(paint_mode);
	doSignalStrengthLoop ();
	hide();

	res = menu_return::RETURN_REPAINT;
	return res;
}

void CStreamInfo2::paint_bitrate(unsigned int br) {
	char buf[100];
	int ypos = y - 5;
	int xpos = x+10;
	int w = w_max (710, 5);
	int ratepos_x = (xpos + w / 2) / 2 + 17;

	ypos += hheight;
	ypos += (iheight >>1);
	ypos += iheight;
	ypos += iheight;
	sprintf((char*) buf, "%5u kbit/s", br);
	frameBuffer->paintBoxRel(xpos, ypos-iheight+1, 300, iheight-1, COL_MENUCONTENT_PLUS_0);
	g_Font[font_info]->RenderString(xpos, ypos, w - 10, g_Locale->getText(LOCALE_STREAMINFO_BITRATE), COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_info]->RenderString(ratepos_x, ypos, w - 10, buf, COL_MENUCONTENT, 0, true); // UTF-8
}

int CStreamInfo2::doSignalStrengthLoop ()
{
	neutrino_msg_t      msg;
	CZapitClient::responseFESignal s;
	int i = 0;
	unsigned int long_average = 0;
#ifdef HAVE_TRIPLEDRAGON
#define INTERVAL 250
#else
#define INTERVAL 100
#endif
	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd_MS(INTERVAL);

	while (1) {
		neutrino_msg_data_t data;

		g_RCInput->getMsgAbsoluteTimeout( &msg, &data, &timeoutEnd );

		/* only do the processing loop if we are not more than 1ms short
		   which can happen if e.g. a key is pressed or a message comes in */
		if((long long)timeoutEnd - CRCInput::calcTimeoutEnd_MS(0) < 1000)
		{
			// -- read signal from Frontend
			g_Zapit->getFESignal(s);

			signal.sig = s.sig & 0xFFFF;
			signal.snr = s.snr & 0xFFFF;
			signal.ber = (s.ber < 0x3FFFF) ? s.ber : 0x3FFFF;  // max. Limit

			if (brc)
				rate.short_average = brc->calc(long_average);

			if (paint_mode == 0 && i == AVERAGE_OVER_X_MEASUREMENTS + 5)
				paint_bitrate(long_average);

			if (i == AVERAGE_OVER_X_MEASUREMENTS + 5)
			{
				if (rate.max_short_average < rate.short_average)
					rate.max_short_average = rate.short_average;
				if (rate.min_short_average > rate.short_average)
					rate.min_short_average = rate.short_average;
				paint_signal_fe(rate, signal);
				signal.old_sig = signal.sig;
				signal.old_snr = signal.snr;
				signal.old_ber = signal.ber;
			}
			else
				i++;

			if (signal.max_ber < signal.ber)
				signal.max_ber = signal.ber;
			if (signal.max_sig < signal.sig)
				signal.max_sig = signal.sig;
			if (signal.max_snr < signal.snr)
				signal.max_snr = signal.snr;

			if (signal.min_ber > signal.ber)
				signal.min_ber = signal.ber;
			if (signal.min_sig > signal.sig)
				signal.min_sig = signal.sig;
			if (signal.min_snr > signal.snr)
				signal.min_snr = signal.snr;

			timeoutEnd = CRCInput::calcTimeoutEnd_MS(INTERVAL);
		}

		// switch paint mode
		if (msg == CRCInput::RC_red) {
			hide ();
			paint_mode = ++paint_mode % 2;
			paint (paint_mode);
			continue;
		}

		// -- key --> abort
		if (msg <= CRCInput::RC_home) {
			break;
		}
		
		// -- push other events
		if ( msg >  CRCInput::RC_home && msg != CRCInput::RC_timeout) {
			CNeutrinoApp::getInstance()->handleMsg( msg, data );
		}
		
	}
	return msg;
}

void CStreamInfo2::hide()
{
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_tv)
		pig->hide();
	frameBuffer->paintBackgroundBoxRel(0,0, max_width,max_height);
}

void CStreamInfo2::paint_pig(int _x, int _y, int w, int h)
{
#if HAVE_DVB_API_VERSION < 3
	frameBuffer->paintBackgroundBoxRel(_x, _y, w, h);
#else
	frameBuffer->paintBoxRel(_x, _y, w, h, COL_BLACK); //black
#endif
#if defined BOXMODEL_DM500 || defined HAVE_IPBOX_HARDWARE
	pig->show (_x-6, _y+3, w, h);
#else
	pig->show(_x, _y, w, h);
#endif
}

void CStreamInfo2::paint_signal_fe_box(int _x, int _y, int w, int h)
{
	int y1, y2;
	int xd = w/4;

	g_Font[font_small]->RenderString(_x, _y+iheight+15, width-10, g_Locale->getText(LOCALE_STREAMINFO_SIGNAL), COL_MENUCONTENT, 0, true);

	sigBox_x = _x;
	sigBox_y = _y+iheight+15;
	sigBox_w = w;
	sigBox_h = h-iheight*3;
	frameBuffer->paintBoxRel(sigBox_x,sigBox_y,sigBox_w+2,sigBox_h, COL_BLACK);

	y1 = _y + h + iheight + iheight+iheight-8;
	y2 = _y + h - sheight+8;
	
	frameBuffer->paintBoxRel(_x+xd*0,y2- 12,16,2, COL_RED); //red
	g_Font[font_small]->RenderString(_x+20+xd*0, y2, 50, "BER", COL_MENUCONTENT, 0, true);

	frameBuffer->paintBoxRel(_x+xd*1,y2- 12,16,2,COL_BLUE); //blue
	g_Font[font_small]->RenderString(_x+20+xd*1, y2, 50, "SNR", COL_MENUCONTENT, 0, true);

	frameBuffer->paintBoxRel(_x+8+xd*2,y2- 12,16,2, COL_GREEN); //green
	g_Font[font_small]->RenderString(_x+28+xd*2, y2, 50, "SIG", COL_MENUCONTENT, 0, true);
	
	frameBuffer->paintBoxRel(_x+xd*3,y2- 12,16,2,COL_OLIVE); // near yellow
	g_Font[font_small]->RenderString(_x+20+xd*3, y2, 50, "Bitrate", COL_MENUCONTENT, 0, true);
	
	sig_text_y = y1 - iheight;
	sig_text_ber_x =  _x +      xd * 0;
	sig_text_snr_x =  _x +  5 + xd * 1;
	sig_text_sig_x =  _x +  5 + xd * 2;
	sig_text_rate_x = _x + 10 + xd * 3;
		
	int maxmin_x; // x-position of min and max
	if (paint_mode == 0) {
		maxmin_x = sig_text_ber_x-40;
	}
	else {
		maxmin_x = _x + 40 + xd * 3 + 45;
	}
	g_Font[font_small]->RenderString(maxmin_x, y1 - sheight - sheight - sheight, 50, "max", COL_MENUCONTENT, 0, true);
	g_Font[font_small]->RenderString(maxmin_x, y1 - sheight, 50, "min", COL_MENUCONTENT, 0, true);


	// --  first draw of dummy signal
	// --  init some values
	{
		sigBox_pos = 0;

		signal.old_sig = 1;
		signal.old_snr = 1;
		signal.old_ber = 1;

	}
}

void CStreamInfo2::paint_signal_fe(struct bitrate br, struct feSignal s)
{
	int   x_now = sigBox_pos;
	int   yt = sig_text_y;
	int   yd;

	sigBox_pos = (++sigBox_pos) % sigBox_w;

	frameBuffer->paintVLine(sigBox_x+sigBox_pos, sigBox_y, sigBox_y+sigBox_h, COL_WHITE);
	frameBuffer->paintVLine(sigBox_x+x_now, sigBox_y, sigBox_y+sigBox_h+1, COL_BLACK);

	SignalRenderStr(br.short_average,     sig_text_rate_x, yt - sheight);
	SignalRenderStr(br.max_short_average, sig_text_rate_x, yt - sheight - sheight);
	SignalRenderStr(br.min_short_average, sig_text_rate_x, yt);
	if ( g_RemoteControl->current_PIDs.PIDs.vpid > 0 ){
		yd = y_signal_fe (rate.short_average, 12000, sigBox_h); // Video + Audio
	} else {
		yd = y_signal_fe (rate.short_average, 512, sigBox_h); // Audio only
	}
	if ((old_x == 0 && old_y == 0) || sigBox_pos == 1) {
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	} else {
		frameBuffer->paintLine(old_x, old_y, sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_OLIVE); //yellow
		old_x = sigBox_x+x_now;
		old_y = sigBox_y+sigBox_h-yd;
	}
	
	if (s.ber != s.old_ber) {
		SignalRenderStr(s.ber,     sig_text_ber_x, yt - sheight);
		SignalRenderStr(s.max_ber, sig_text_ber_x, yt - sheight - sheight);
		SignalRenderStr(s.min_ber, sig_text_ber_x, yt);
	}
	yd = y_signal_fe (s.ber, 4000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_RED); //red


	if (s.sig != s.old_sig) {
		SignalRenderStr(s.sig,     sig_text_sig_x, yt - sheight);
		SignalRenderStr(s.max_sig, sig_text_sig_x, yt - sheight - sheight);
		SignalRenderStr(s.min_sig, sig_text_sig_x, yt);
	}
	yd = y_signal_fe (s.sig, 65000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_GREEN); //green


	if (s.snr != s.old_snr) {
		SignalRenderStr(s.snr,     sig_text_snr_x, yt - sheight);
		SignalRenderStr(s.max_snr, sig_text_snr_x, yt - sheight - sheight);
		SignalRenderStr(s.min_snr, sig_text_snr_x, yt);
	}
	yd = y_signal_fe (s.snr, 65000, sigBox_h);
	frameBuffer->paintPixel(sigBox_x+x_now, sigBox_y+sigBox_h-yd, COL_BLUE); //blue
}


// -- calc y from max_range and max_y
int CStreamInfo2::y_signal_fe(unsigned long value, unsigned long max_value, int max_y)
{
	long  l;

	if (!max_value) max_value = 1;

	l = ((long) max_y * (long) value ) / (long) max_value;
	if (l > max_y) l = max_y;

	return (int) l;
}

void CStreamInfo2::SignalRenderStr(unsigned int value, int _x, int _y)
{
	char str[30];

	frameBuffer->paintBoxRel(_x, _y - sheight + 5, 60, sheight - 1, COL_MENUCONTENT_PLUS_0);
	sprintf(str,"%6u",value);
	g_Font[font_small]->RenderString(_x, _y + 5, 60, str, COL_MENUCONTENT, 0, true);
}

struct button_label StreamInfoButtons[2] =
{
	{ NEUTRINO_ICON_BUTTON_RED  , LOCALE_GENERIC_EMPTY      },
	{ NEUTRINO_ICON_BUTTON_HOME , LOCALE_STREAMINFO_CLOSE	}
};

void CStreamInfo2::paint(int/*mode*/)
{
	const char * head_string;
	int ypos = y+5;
	int xpos = x+10;

	if (paint_mode == 0) {

		// -- tech Infos, PIG, small signal graph
		head_string = g_Locale->getText(LOCALE_STREAMINFO_HEAD);
		CLCD::getInstance()->setMode(CLCD::MODE_MENU_UTF8, head_string);
		
		// paint backround, title pig, etc.
		int background_x = xpos-10;
		int background_w = width+10; 
		int background_h = height-hheight-sheight ;
		int pigboxes_x = xpos+width-260;
		
		frameBuffer->paintBoxRel(background_x, y, background_w, hheight, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP);
		g_Font[font_head]->RenderString(xpos, y + hheight + 2, width, head_string, COL_MENUHEAD, 0, true); // UTF-8	
		frameBuffer->paintBoxRel(background_x, y + hheight, background_w, background_h, COL_MENUCONTENT_PLUS_0);
		ypos = y+hheight+8;

		// paint PIG
		if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_tv)
		{
#if defined BOXMODEL_DM500 || defined HAVE_IPBOX_HARDWARE
			// the dm500 seems to like only half / quarter resolution...
			paint_pig(pigboxes_x + 60, ypos, 180, 144);
#else
			paint_pig(pigboxes_x, ypos, 240, 190);
#endif
		}

		// Info Output
		paint_techinfo(xpos, ypos);

		paint_signal_fe_box(pigboxes_x, ypos + 185, 240, 190);
		
		// buttons
		int button_y = ypos+background_h-8;
		int ButtonWidth = background_w/3;
		StreamInfoButtons[0].locale = LOCALE_STREAMINFO_MAXIMIZE;
		
		frameBuffer->paintBoxRel(background_x, button_y, background_w, sheight , COL_INFOBAR_SHADOW_PLUS_1, RADIUS_MID, CORNER_BOTTOM);
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, xpos + 4, button_y, ButtonWidth, 1, &StreamInfoButtons[0]);
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, xpos + 4 + 2*ButtonWidth, button_y, ButtonWidth, 1, &StreamInfoButtons[1]);

	} else {

		// --  small PIG, small signal graph

		// -- paint backround, title pig, etc.
		frameBuffer->paintBoxRel(0, 0, max_width, max_height, COL_MENUCONTENT_PLUS_0);

		// -- paint large signal graph
		paint_signal_fe_box(x, y, width, height - 100);
		
		// -- buttons
		int button_y = y + height;
		int ButtonWidth = width / 2;

		StreamInfoButtons[0].locale = LOCALE_STREAMINFO_RESIZE;
		
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, xpos, button_y - 20, ButtonWidth, 1, &StreamInfoButtons[0], 0, false, COL_MENUCONTENT_PLUS_0);
		::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, width - 110, button_y - 20, ButtonWidth, 1, &StreamInfoButtons[1], 0, false, COL_MENUCONTENT_PLUS_0);
	}

}

void CStreamInfo2::paint_techinfo(int xpos, int ypos)
{
	// Info Output
	char buf[100], buf2[100];
	int count = 0;
	long bitInfo[10];
	const char *framerates[] = { "",	// 0
				     "23.976",	// 1
				     "24fps",	// 2
				     "25fps",	// 3
				     "29.97",	// 4
				     "30fps",	// 5
				     "50fps",	// 6
				     "59.94",	// 7
				     "60fps" };	// 8
#ifndef HAVE_TRIPLEDRAGON
	FILE* fd = fopen("/proc/bus/bitstream", "rt");
	if (fd==NULL)
	{
		printf("error while opening proc-bitstream\n" );
		return;
	}

	char *key,*tmpptr;
	long value;
	int pos=0;
	fgets(buf,35,fd);//dummy
	while(!feof(fd))
	{
		if(fgets(buf,99,fd)!=NULL)
		{
			buf[strlen(buf)-1]=0;
			tmpptr=buf;
			key=strsep(&tmpptr,":");
			value=strtoul(tmpptr,NULL,0);
			bitInfo[pos]= value;
			pos++;
		}
	}
	fclose(fd);
#else
	/* set the bitInfo fields similar to the dbox... values are from
	   the videodecoder, who gets them from the stream... */
	AVInfo v = g_Zapit->getAVInfo();
	bitInfo[0] = (long)v.vinfo.h_size;
	bitInfo[1] = (long)v.vinfo.v_size;
	bitInfo[2] = (long)v.vinfo.pel_aspect_ratio + 2; // td has 0,1,2 dbox has 2,3,4
	bitInfo[3] = (long)v.vinfo.frame_rate;
	if (v.atype != 0)
		bitInfo[7] = 0;
	else
		bitInfo[7] = v.astatus.word00;
#endif
	int outputpos_x = (xpos + width/2)/2+25;
	
	ypos+= iheight;
	sprintf((char*) buf, "%dx%d", (int)bitInfo[0], (int)bitInfo[1] );
	g_Font[font_info]->RenderString(xpos, ypos, width-10, g_Locale->getText(LOCALE_STREAMINFO_RESOLUTION), COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_info]->RenderString(outputpos_x, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8


	ypos += iheight;
//	sprintf((char*) buf, "%s: %d bits/sec", g_Locale->getText(LOCALE_STREAMINFO_BITRATE), (int)bitInfo[4]*50);
//	g_Font[font_info]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8


	ypos += iheight;
	switch (bitInfo[2])
	{
		case 2:
			sprintf((char*) buf, "4:3");
			break;
		case 3:
			sprintf((char*) buf, "16:9");
			break;
		case 4:
			sprintf((char*) buf, "2.21:1");
			break;
		default:
			strncpy(buf, g_Locale->getText(LOCALE_STREAMINFO_ARATIO_UNKNOWN), sizeof(buf));
	}
	g_Font[font_info]->RenderString(xpos, ypos, width-10,  g_Locale->getText(LOCALE_STREAMINFO_ARATIO), COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_info]->RenderString(outputpos_x, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8



	ypos+= iheight;
	if (bitInfo[3] < 1 || bitInfo[3] > 8)
		strncpy(buf, g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE_UNKNOWN), sizeof(buf));
	else
		strcpy(buf, framerates[bitInfo[3]]);

	g_Font[font_info]->RenderString(xpos, ypos, width-10, g_Locale->getText(LOCALE_STREAMINFO_FRAMERATE), COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_info]->RenderString(outputpos_x, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8



       if (!bitInfo[7])
              strncpy(buf, g_Locale->getText(LOCALE_STREAMINFO_AUDIOTYPE_UNKNOWN), sizeof(buf));
	else {
		const char* layernames[4]	={"res", "III", "II", "I"};
		const char* sampfreqnames[4]	={"44,1k", "48k", "32k", "res"};
		const char* modenames[4]	={"stereo","joint_st", "dual_ch", "single_ch"};

		long header = bitInfo[7];

		unsigned char layer 	= (header>>17) & 3;
		unsigned char sampfreq 	= (header>>10) & 3;
		unsigned char mode 	= (header>> 6) & 3;
		unsigned char copy 	= (header>> 3) & 1;

		sprintf((char*) buf, "%s (%s/%s) %s",
								modenames[mode],
								sampfreqnames[sampfreq],
								layernames[layer],
								copy ? "c" : "");
	}
	g_Font[font_info]->RenderString(xpos, ypos+ iheight, width-10, g_Locale->getText(LOCALE_STREAMINFO_AUDIOTYPE), COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_info]->RenderString(outputpos_x, ypos+ iheight, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
	ypos+= iheight+ 10;

	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();
	
	// paint labels
	int spaceoffset = 70;
	
	//tsfrequenz
	ypos+= sheight-5; //blank line
	if (g_info.delivery_system == DVB_S)
		sprintf((char*) buf, "%d.%03d MHz (%c)", si.tsfrequency / 1000, si.tsfrequency % 1000, (si.polarisation == HORIZONTAL) ? 'h' : 'v');
	else
		sprintf((char*) buf, "%d.%06d MHz", si.tsfrequency / 1000000, si.tsfrequency % 1000000);
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "Freq:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8	

	//onid
	ypos+= sheight;
	sprintf((char*) buf, "0x%04x", si.onid);
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "ONid:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

	//sid
	ypos+= sheight;
	sprintf((char*) buf, "0x%04x", si.sid);
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "Sid:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

	//tsid
	ypos+= sheight;
	sprintf((char*) buf, "0x%04x", si.tsid);
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "TSid:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
	
	//pmtpid
	ypos+= sheight+5; //blank line
	if (si.pmtpid != 0)
		sprintf((char*) buf, "0x%04x", si.pmtpid);
	else
		sprintf((char*) buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "PMTpid:", COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8 

	//pcrpid
	ypos+= sheight;
	if (si.pcrpid != 0)
		sprintf((char*) buf, "0x%04x", si.pcrpid);
	else
		sprintf((char*) buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "PCRpid:", COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

	//vpid
	ypos+= sheight;
	if ( g_RemoteControl->current_PIDs.PIDs.vpid > 0 ){
		sprintf((char*) buf, "0x%04x", g_RemoteControl->current_PIDs.PIDs.vpid );
	} else {
		sprintf((char*) buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	}
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "Vpid:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

	//apid	
	if (g_RemoteControl->current_PIDs.APIDs.empty()){
		ypos+= sheight;
		sprintf((char*) buf, "Apid(s):  %s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
		g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
	} else {
		unsigned int i;
		sprintf((char*) buf, "Apid(s):" );
		for (i= 0; (i<g_RemoteControl->current_PIDs.APIDs.size()) && (i<10); i++)
		{
			if (i == g_RemoteControl->current_PIDs.PIDs.selected_apid)
				sprintf((char*) buf2, "  <0x%04x>",  g_RemoteControl->current_PIDs.APIDs[i].pid );
			else	
				sprintf((char*) buf2, "  0x%04x",  g_RemoteControl->current_PIDs.APIDs[i].pid );
			if ((i > 0) && (i%4 != 0))
			{
				strcat((char*) buf, ",");
			}
			strcat((char*) buf, buf2);
			if ((i+1)%4 == 0) // if we have lots of apids, put "intermediate" line with pids
			{
				ypos+= sheight;
				g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
				sprintf((char*) buf, "           " );
			}
		}
		if ((i)%4 != 0) // put finishing (and only?) line with apids if not ended with an intermediate line
		{
			ypos+= sheight;
			g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
		}
	}

	//vtxtpid
	ypos += sheight;
	if ( g_RemoteControl->current_PIDs.PIDs.vtxtpid == 0 )
        	sprintf((char*) buf, "%s", g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE));
	else
        	sprintf((char*) buf, "0x%04x", g_RemoteControl->current_PIDs.PIDs.vtxtpid );
	g_Font[font_small]->RenderString(xpos, ypos, width-10, "VTXTpid:" , COL_MENUCONTENT, 0, true); // UTF-8
	g_Font[font_small]->RenderString(xpos+spaceoffset, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

	// Subtitle pids
	ypos+= sheight+5;
	snprintf((char*)buf, sizeof(buf), "%s: ", "Sub pid(s)");
	strcpy(buf2, "");
	count=0;
	for (unsigned i = 0 ;
		i < g_RemoteControl->current_PIDs.SubPIDs.size() ; i++) {
		if (g_RemoteControl->current_PIDs.SubPIDs[i].pid !=
			g_RemoteControl->current_PIDs.PIDs.vtxtpid) {
			char tmpbuf[100];
			if (*buf2) {
				strncat(buf2, ", ", sizeof(buf2));
			}
			snprintf(tmpbuf, sizeof(tmpbuf), "0x%04x %s",
				g_RemoteControl->current_PIDs.SubPIDs[i].pid,
				g_RemoteControl->current_PIDs.SubPIDs[i].desc);
			strncat(buf2, tmpbuf, sizeof(buf2));
			if (++count == 2) {
				strncat(buf, buf2, sizeof(buf));
				g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
				ypos += sheight;
				strcpy(buf, "          ");
				strcpy(buf2, "");
			}
		}
	}
	if (count) {
		strncat(buf, buf2, sizeof(buf));
	} else {
		strncat(buf,
			g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE),
			sizeof(buf));
	}
	if (count != 2) {
		g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
        	ypos += sheight;
	}

	// TTX subtitles
	snprintf((char*)buf, sizeof(buf), "%s: ", "TTXsub page(s)");
	strcpy(buf2, "");
	count = 0;
	for (unsigned i = 0; i < g_RemoteControl->current_PIDs.SubPIDs.size(); i++) {
		if (g_RemoteControl->current_PIDs.SubPIDs[i].pid == g_RemoteControl->current_PIDs.PIDs.vtxtpid) {
			char tmpbuf[100];
			if (*buf2) {
				strncat(buf2, ", ", sizeof(buf2));
			}
			snprintf(tmpbuf, sizeof(tmpbuf), "%03d %s",
				g_RemoteControl->current_PIDs.SubPIDs[i].composition_page,
				g_RemoteControl->current_PIDs.SubPIDs[i].desc);
			strncat(buf2, tmpbuf, sizeof(buf2));
			if (++count == 3) {
				strncat(buf, buf2, sizeof(buf));
				g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
				ypos += sheight;
				strcpy(buf, "          ");
				strcpy(buf2, "");
			}
		}
	}
	if (count) {
		strncat(buf, buf2, sizeof(buf));
	} else {
		strncat(buf,
			g_Locale->getText(LOCALE_STREAMINFO_NOT_AVAILABLE),
			sizeof(buf));
	}
	if (count != 3) {
		g_Font[font_small]->RenderString(x+ 10, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
		ypos+= sheight;
	}

	//satellite
	ypos += 5;
	const char* pr_sys = (g_info.delivery_system == DVB_S ? "Satellite: %s" : "Provider: %s");
	sprintf((char*) buf, pr_sys ,CNeutrinoApp::getInstance()->getScanSettings().satOfDiseqc(si.diseqc));
	std::cout<<"[streaminfo] "<< buf <<std::endl;
	g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8
	
	//channel
	ypos += sheight;
	sprintf((char*) buf, "Channel: %s" ,g_RemoteControl->getCurrentChannelName().c_str());
	std::cout<<"[streaminfo] "<< buf<<std::endl;
	g_Font[font_small]->RenderString(xpos, ypos, width-10, buf, COL_MENUCONTENT, 0, true); // UTF-8

}

/************************************************************************/
std::string CStreamInfo2Misc::getStreamInfoVersion(void)
{	
	static CImageInfo imageinfo;
	return imageinfo.getModulVersion("","$Revision: 1.50 $");
}

int CStreamInfo2Handler::exec(CMenuTarget* parent, const std::string &)
{
	int res = menu_return::RETURN_EXIT_ALL;
	if (parent)
	{
		parent->hide();
	}
	CStreamInfo2 *e = new CStreamInfo2;
	e->exec();
	delete e;
	return res;
}
