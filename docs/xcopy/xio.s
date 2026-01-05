	INCLUDE "xcopy.i"
	INCLUDE "gfxlib.i"
;	INCLUDE "INCLUDE:offsets.i"

 IFEQ SOLO
	OUTPUT	"XCopy"
 ELSE
	OUTPUT	"solo"
 ENDC
	MC68000

PIC_SIZE = 40000

;DCACHEOFF = 0
;
; ** 12/10/91: DiskChange routine berichtigt. DRIVESON Item eingefügt

; **  8/11/90: Bug in CheckMem behoben und DelayTime fuer Keyboard-
; **           Handshake erhoeht. Neu: Farbaenderung der Maus bei Freeze.
;

; CHANGED (H.V):
;  Assembler: Macro68/BLink	Aztec sucks (hab keinen 040!)
;  Input			Kein Flickern mehr
;  cli_startup			move.w #$ffff,$4c(a0) removed (OS-2.0 crash)
;  Novirus Bootblock		Copperlist waits gefixed (jetzt durchgäng.linien)
;  Startup-Bug gefixed		Wartet beim Start bis drives fertig sind
;  Mouseroutine gefixed		Wenn die Maus langsam bewegt wird, gehts jetzt auch
;  Sound gefixed		Wenn man während Sound stop drüeckt gabs noise
;  ChipMem Buf gefixed		Erkennt jetzt auch >512K Chip korrekt

; ADDED (H.V):
;  StatusInput			Löscht die Statuszeile nicht, String+" "
;  Symbol: "GER"		Wenn definiert (IDF GER) dann deutsche Texte


; REMOVED
;  IFD AZTEC stuff

;---------------------------------------------------------------------------

; ** Adress-Definitionen fuer CIA B

CB_TALO	= $BFD400	; Timer A LOW-Byte
CB_TAHI	= $BFD500	; Timer A HIGH-Byte
CB_TBLO	= $BFD600	; Timer B LOW-Byte
CB_TBHI	= $BFD700	; Timer B HIGH-Byte
CB_ICR	= $BFDD00	; Interrupt control register
CB_CRA	= $BFDE00	; Control Register A
CB_CRB	= $BFDF00

; ** Adress-Definitionen fuer CIA A

CA_SDR	= $BFEC01	; Keyboard-Register
CA_ICR	= $BFED01	; Interrupt control register
CA_CRA	= $BFEE01	; Control Register A

;---------------------------------------------------------------------------
; ** Diverse Konstanten

dumy	= 0	; Speicherstelle NULL
SPH	= 9	; SpriteHoehe
DTIME	= 1

TPS	= 50	; Ticks per Second


; **********************************************************************

; **********************************************************************
; **
; ** XCOPY STARTUP Code	

pr_MsgPort	=  92	; Offsets in die Prozessstruktur eines Tasks
pr_CLI	= 172

FindTask	= -294	; Exec-Funktionen
FindName	= -276
Forbid	= -132
GetMsg	= -372
ReplyMsg	= -378
WaitPort	= -384
OpenResource = -498
OpenLibrary = -408
;OpenDevice = -444
;CloseDevice = -450

DisplayAlert = -90



	CODESEG

; ** Einsprungpunkt fuer das Programm

start:

 IFEQ SOLO
	bra.s	askip
confflag	DC.W	0
_deflen	DC.L	_enddefault-_default
_default:
	DC.W	0,79,0,1,0	; starttrack,endtrack,starthead,endhead,side
	DC.W	DOSCOPY,0,$4489	; mode,device,sync
	DC.B	1,2,2,0		; Source,Target,Verify,OldVerify
_enddefault:

askip 	bsr	VBRFix
;	move.l	SetupID,D0	; wird XCOPY von Setup-Prg gerufen ?
;	cmp.l	$100,D0
;	beq.s	cli_startup	; JA

	move.l	4,A6		; SysBase holen
	tst.w	confflag
	bne.s	cli_startup
	sub.l	A1,A1		; FindTask(0)
	jsr	FindTask(A6)
	move.l	D0,A1		; Pointer auf eigenen Prozess
	tst.l	pr_CLI(A1)	; wird XCOPY vom CLI gerufen
	bne.s	cli_startup	; JA

	move.l	A1,-(SP)	; XCOPY wird von WorkBench gerufen
	lea	pr_MsgPort(A1),A0	
	jsr	WaitPort(A6)	; bei Workbench-Startup auf Message warten
	move.l	(SP)+,A1
	lea	pr_MsgPort(A1),A0
	jsr	GetMsg(A6)
	move.l	D0,WBenchMsg	; Pointer auf Startup-Message retten
 ELSE
 				; bei SOLO Parameter aus MULTI ubernehmen
	movem.l	d1-d7/a1-a2,default
	movem.l	d1-d7/a1-a2,default
	bra	go1
;	movem.l	d1-d7/a1-a2,default
;	lea	RETADR,A0
;	move.l	D7,(A0)	; Return-Adresse speichern
 ENDC

cli_startup:
 IFEQ SOLO
	lea	VarBasis+$8000,a4
	BASEREG	VarBasis+$8000,a4
	moveq	#3,d7		; seek to track-0, via trackdisk
.seek0	move.l	d7,d0
	bsr	td_Seek0
	dbf	d7,.seek0
	;
	bsr	CountDrives
	;
 ENDC

	lea	go1(pc),A0	; das eigentliche Programm ausfuehren	
	move.l	A0,$80
	trap	#0

 IFEQ SOLO


	move.l	4,A6		; alle Trackpositionen in trackdisk.device
	lea	trackname,A1	; als ungueltig (-1) markieren
	lea	350(A6),A0	; Offset von DeviceList in ExecBase
	jsr	FindName(A6)	; trackdisk.device suchen
	tst.l	D0
	beq.s	tstwb
	move.l	D0,A0		; Pointer auf Device-Struktur
	lea	$24(A0),A0	; hier stehen die UNIT-Pointer
	moveq	#3,D0		; maximal 4 Laufwerke
	lea	ondrives,a2
	lea	drivetracks,a3
1$	move.l	(A0)+,D1
	beq.s	2$
	move.l	D1,A1
;	move.w	#$FFFF,$4C(A1)	; REMOVED!!! OS2.0 wird böse!! (H.V 17-9-91)
	move.b	#1,(a3)
	move.b	#1,(a2)		; hier steht die aktuelle Kopfposition eines
2$	addq.l	#1,a2
	addq.l	#2,a3
	dbf	D0,1$		; Laufwerks

	bsr	ReInitDrives

tstwb	move.l	4,a6
	tst.l	WBenchMsg		
	beq.s	ex2dos		; vom CLI gestartet
	jsr	Forbid(A6)
	move.l	WBenchMsg,A1	; Workbench-Message zurueckschicken
	jsr	ReplyMsg(A6)
 
ex2dos	tst.w	confflag
	beq.s	.exit
	move.l	_deflen,d0
	subq.l	#1,d0
	lea	starttrack,a0
	lea	_default,a1
.copy	move.b	(a0)+,(a1)+
	dbf	d0,.copy
 ENDC

.exit	moveq	#0,D0	; kein Error
	rts
WBenchMsg	DS.L	1

	CODESEG
	EVEN


; ** end of startup code
		; Wertebereich der Variablen
starttrack	DC.W	0	; 0 - 81
endtrack	DC.W	79	; 0 - 81
starthead	DC.W	0 	; 0 - 1
endhead		DC.W	1	; 0 - 1
side		DC.W	0	; 0 = BOTH, 1 = UPPER, 2 = LOWER

mode		DC.W	0	; 
device		DC.W	0	; 0 = DISK, 1 = RAM
sync		DC.W	$4489	; $F8BC = INDX, sonst Wert

Source		DC.B	0	; 1 = DF0:, 2 = DF1:, 4 = DF2:, 8 = DF3:
Target		DC.B	0	; wie Source
Verify		DC.B	0	; wie Source, bis hier Default-Area
OldVerify	DC.B	0	; wie Source
	EVEN
	BSSSEG
	
VarBasis:

oldblock	DS.L	1
oldindex	DS.L	1
oldport		DS.L	1
irqenable	DS.W	1
dmaenable	DS.W	1
	CODESEG

 IFEQ SOLO
	BSSSEG
Stack	DS.L	1
GfxBase	DS.L	1
	CODESEG
GfxName		DC.B "graphics.library",0,$ff
dskres		DC.B "disk.resource",0,$ff
trackname	DC.B "trackdisk.device",0,$ff
	EVEN
 ENDC
 IFNE SOLO
	BSSSEG
ChipMemLow	DS.L	1
ChipMemHigh 	DS.L	1
FastMemLow	DS.L	1
FastMemHigh 	DS.L	1
MemPTH		DS.L	1
	CODESEG
 ENDC


; **************************************************************
; **
; ** Ab hier wird das gepackte Menue abgelegt bzw. das gepackte
; ** SOLO-Copy. Der Speicher vom MENUE wird nach dem entpacken
; ** als Puffer fuer diverse Zwecke benutzt.

 IFEQ SOLO
seroffs:
	DC.W (serialtxt+14)-seroffs
 ENDC

	CODESEG
;-----------------------------------------------------
;
; ** Initialisierungs- und Exit-Code fuer MULTI-Version

 IFEQ SOLO


;	***----------------------------------
;	*** VBR Fix (by Carnivore/BeerMacht)
;	***----------------------------------


VBRFix:	move.l	4,A6
	btst	#0,$129(A6)		;68010 ?
	beq.s	.no6810
	lea	.vbrmove(pc),A5
	jsr	-$1E(a6)		;supervisor()
.no6810	rts
	;
	MC68010
.vbrmove
	movec	VBR,A1			;move vbr back down
	cmp.l	#0,a1
	beq.s	.end
	sub.l	A2,A2
	moveq	#$7F,D7
.move	move.l	(A1)+,(A2)+
	move.l	(A1)+,(A2)+
	dbra	D7,.move
	moveq	#0,D7
	movec	D7,VBR
	;
 IFD DCACHEOFF
	btst	#1,$129(a6)		;68020 ?
	beq.s	.end
	movec	CACR,D7
	and.l	#$FFFFEEFF,D7		;data cache off
	movec	D7,CACR
 ENDC
.end	rte
	;
vbrloc	DC.L	0
	;
	MC68000

;	***-----------
;	***  G    O
;	***-----------

go1	move.l	A7,Stack	; Stack-Pointer zwischenspeichern
	move.w	#$2000,SR
	move.l	4,A6
	moveq	#0,D0
	lea	GfxName,A1
	moveq	#0,D0
	jsr	-552(A6)	; Open GfxLib
	move.l	D0,GfxBase
	move.l	D0,A1	; GfxLib gleich wieder schliessen
	jsr	-414(A6)

	move.l	#PIC_SIZE,D0	; CHIP_MEM fuer BitMap holen
	moveq	#2,D1
	bsr	AllocMem
	move.l	D0,planeptr
	beq	Bye
	move.l	d0,a1				;destination
	lea	xcopypic,a0			;packed source
	bsr	PPDecrunch	; Grafik entpacken und nach BitMap

;	bsr	CountDrives
	bra	SetUP

; **

Bye:
 IFD DOSETUP
	move.l	SetupID,D0
	cmp.l	$100,D0	; Setup-Version (DSET) ?
	bne	nosetup	; nein

	movem.l	a0-a1,-(a7)	; ja, Parameter fuer das Setup-PRG
	lea	starttrack,a0	; zwischenspeichern
	lea	$200,A1
	moveq	#24-1,D0
1$	move.b	(A0)+,(A1)+
	dbf	D0,1$
	move.l	#$444f4e45,$100	; DONE
	movem.l	(A7)+,A0-A1
 ENDC
nosetup
;	bsr	CheckEqualDisks

	moveq	#0,d0
	move.b	StartupDrives,d0
	bsr	seldri
	bsr	track0


	lea	$DFF000,A6
	move.l	planeptr,d0
	beq.s	quit
	move.l	d0,a1
	move.l	#PIC_SIZE,D0
	bsr	FreeMem
	tst.w	dmaenable	; DisplayInit wurde noch nicht aufgerufen
	beq.s	quit
	move.w	#$7fff,D0
	move.w	D0,$9C(A6)	;clear Request
	move.w	D0,$9A(A6)	;clear all enable bits
	move.w	D0,$96(A6)
	bsr	ResetCIAB
	move.l	oldindex,$78
	move.l	oldblock,$64
	move.l	oldport,$68
	move.l	GfxBase,A0	; 
	move.l	38(A0),$80(A6)	; CopperListe an, GfxBase->CopInit
	move.w	D0,$88(A6)	
	move.w	dmaenable,D0
	or.w	#$8200,D0
	move.w	D0,$96(A6)	; enable DMA Channels
	move.w	irqenable,D0
	or.w	#$C000,D0
	move.w	D0,$9A(A6)	; enable IRQS
quit	move.w	#$2000,SR
	move.l	Stack,A7
	rte

SAVEREGS	MACRO
	movem.l	D1-D7/A0-A6,-(A7)
	ENDM
LOADREGS	MACRO
	movem.l	(A7)+,D1-D7/A0-A6
	ENDM

AvailMem:
	SAVEREGS
	move.l	4,A6
	jsr	-216(A6)	; Exec AvailMem
	LOADREGS
	rts

AllocMem
	SAVEREGS
	move.l	4,A6
	jsr	-198(A6)	; Exec AllocMem
	LOADREGS
	rts

FreeMem	SAVEREGS
	move.l	4,A6
	jsr	-210(A6)	; Exec FreeMem
	LOADREGS
	rts


;	*** CountDrives
;	*** -----------
CountDrives:
	move.l	4,A6
	lea	dskres,A1
	moveq	#0,D0
	jsr	OpenResource(A6)
	tst.l	D0
	beq	Bye
	move.l	D0,A0
	lea	48(A0),A1	; dr_UnitID
	moveq	#0,D0
	moveq	#0,D1
1$	tst.l	(A1)+
	bne.s	2$
	bset	D1,D0
2$	addq.w	#1,D1
	cmp.w	#4,D1
	blt.s	1$	
	move.b	D0,Drives
	move.b	d0,StartupDrives
3$	move.b	38(A0),D1	; dr_Flags abfragen
	btst	#7,D1
	bne.s	3$
	rts
 ENDC
;*******************
;-----------------------------------------------------
;
; ** Initialisierungs- und Exit-Code fuer SOLO-Version
; ** Die aktuelle Speicherbelegung von SOLO hat folgende Gestalt:
;
; ** $0000-$00BF  IRQ/TRAP-Vektoren
; ** $00C0-$03ff  Supervisor-Stack => koennte ev. zu klein sein
; ** $0400-$B17C  Programm und Daten, reserviert ist aber bis $C100
; ** $C100-$CC0A  MemTab, reserviert ist bis $D000
; ** $D000-       Pufferspeicher   


 IFNE SOLO



;	xdef MemPTH

go1:	bsr	CreateCopper
	move.l	#SOLOSTART,A7	; Supervisorstack von $0c0 bis $400
	move.w	#$2000,SR
	lea	BitMAP,A0
	move.l	A0,planeptr	; Speichercheck
	bsr	CheckMem	
	bra	SetUP

Bye
;	move.l	#$300,A0
;	move.l	ChipMemLow,(A0)+ ; z.Z auf $D000
;	move.l	ChipMemHigh,(A0)+
;	move.l	FastMemLow,(A0)+
;	move.l	FastMemHigh,(A0)+
;	move.l	MemPTH,(A0)+
;	move.l	mskpth,(A0)+	; z.Z auf $ADf8

;	move.l	mfm1,(A0)+
;	move.l	mfm2,(A0)+
;	move.l	mfm3,(A0)+
;	move.w	#$7fff,D0
;	move.w	D0,$9C(A6)	;clear Request
;	move.w	D0,$9A(A6)	;clear all enable bits
;	move.w	D0,$96(A6)
	jmp	$fc0002
	DC.W	$4ef9	; jmp	OP-Code
;RETADR	DC.L	0	; jmp	$FC0002

AvailMem
	move.l	ChipMemHigh,D0	; freies CHIP_MEM
	sub.l	ChipMemLow,D0
	tst.l	FastMemLow
	beq.s	1$
	add.l	FastMemHigh,D0		
	sub.l	FastMemLow,D0
1$	rts

AllocMem
	movem.l	A0-A1,-(SP)
	move.l	MemPTH,A1	; zeigt auf Beginn des freien Speichers
	move.l	ChipMemHigh,A0	; ChipMemHigh
	sub.l	D0,A0		; angeforderte Groesse abziehen
	cmp.l	A0,A1	
	bls.s	mem_fou
	tst.l	FastMemLow
	beq	no_mem
	cmp.l	FastMemLow,A1
	bhi.s	check_fast
	move.l	FastMemLow,A1	; erster Block im FAST_MEM
	bra.s	mem_fou
check_fast
	move.l	FastMemHigh,A0	; FastMemHigh
	sub.l	D0,A0
	cmp.l	A0,A1	
	bls.s	mem_fou
no_mem	movem.l	(SP)+,A0-A1
	moveq	#0,D0
	rts
mem_fou	move.l	A1,MemPTH
	add.l	D0,MemPTH	; Zeiger erhoehen
	move.l	A1,D0		; A1 -> Pointer auf freien Block
	movem.l	(SP)+,A0-A1
	rts

FreeMem	move.l	ChipMemLow,MemPTH
	rts			; gibt ALLEN Speicher frei!!

;	*** CheckMem
;	*** --------
CheckMem:
	move.l	#BUFFERPOOL+$F00,ChipMemLow ; $C100+$0F00 = Arbeitsspeicher	
	move.l	maxchip,ChipMemHigh
	bsr	FreeMem
	lea	FastMemLow,a0
	move.l	lofast,(a0)+
	move.l	hifast,(a0)
	rts


 ENDC
;*******************

;-----------------------------------------------------
;
; ** Initialisierung der Hardware fuer MULTI- und SOLO-Version

SetUP:
	move.l	#BUFFERPOOL,MemTab
	move.l	#WORKTAB,WorkTab
	move.l	MemTab,A0	; Grafik loeschen, da nun Variablenspeicher
	move.w	#$2000/4-1,D0	; 8192 Bytes loeschen
2$	clr.l	(A0)+
	dbf	D0,2$
	bsr	CreateMask	

	lea	coplist,A0
	lea	sprite,A1	
	move.l	A1,D0		; Sprite Pointer in coplist schreiben
	move.w	D0,38(A0)	; Sprite 5 benutzten
	swap	D0
	move.w	D0,34(A0) 		

;	add.l	#102,A0		; Bitmappointer in coplist schreiben
	lea	cop_planes+2,a0
	move.l	planeptr,A2
	moveq	#DEP-1,D0	; 5 Planes
1$	move.l	A2,D1
	move.w	D1,4(A0)
	swap	D1
	move.w	D1,(A0)
	addq.l	#8,A0
	add.w	#BPR,A2
	dbf	D0,1$

	clr.w	copy_active
	clr.w	oldx
	clr.w	oldy
	clr.w	spx
	clr.w	spy
	move.b	#DTIME,dely
	lea	$DFF000,A6

 IFEQ SOLO
	move.l	$78,oldindex	; bei SOLO Vektoren nicht sichern
	move.l	$64,oldblock	; da eh ein RESET durchgefuehrt wird
	move.l	$68,oldport
	move.w	$02(A6),dmaenable
	move.w	$1C(A6),irqenable ; save enable Bits
 ENDC

	move.w	#$7fff,D0
	move.w	D0,$9C(A6)	; clear Request
	move.w	D0,$9A(A6)	; clear all enable bits
	move.w	D0,$96(A6)	; DMA off

	clr.w	$24(A6)	; DSKLEN

	bsr	ResetCIAB	; CIA Reset,dann IRQ-Vektoren setzen
	move.l	#CIAB_server,$78
	move.l	#DSKBLKdone,$64
	move.l	#port,$68

	lea	$180(A6),A0
	lea	Cols,A1
	moveq	#31,D0
3$	move.w	(A1)+,(A0)+		
	dbf	D0,3$
	bsr	FlipMouseColors
	clr.l	dumy		; Dummy Spritepointer auf NULL	
	lea	coplist,A0	; Pointer auf Copperliste
	move.l	A0,$80(A6)	; CopperListe an
	move.w	D0,$88(A6)	
	move.w	#$83F0,$96(A6)	; enable DMA Channels
	move.w	#$E00A,$9A(A6)	; enable INDEX,(VBI),DISKBLOCK,PORT

	SetTextMode NOBACKGND!NOSHADOW ; initialisiert Variablen fuer x_Print
	bsr	DrivesOFF

	bsr	SetDefault

;	bsr	Versionnr
	bsr	TimeOut		; Zeit zum ersten mal ausgeben (00:00)
	move.b	#$FE,CB_TBLO	; Timer B in CIA B loest alle 20000 us 
	move.b	#$36,CB_TBHI	; einen IRQ aus (Pseudo-VBL)
	move.b	#1,CB_CRB	; Timer B Start, continuous mode
	move.b	#$82,CB_ICR	; IRQ fuer Timer B zulassen
	move.b	#2,I_enable	; auch Bit in System-Maske setzen
 IFEQ SOLO	; Copyright bei MULTI-Version ausgeben
;	bsr	Copyright
 ENDC
	bra	IconHandler
;---------------------------------------------------------------------------
;
FlipMouseColors:
	movem.l	D0-D1/A0-A1,-(SP) 
	lea	normcols,A1
	move.l	#$DFF180+25*2,A0	; Farben 25-27 neu setzen 	
	moveq	#2,D0			; 3 farben insgesamt	
3$	move.w	(A1),D1			; Farbe holen
	move.w	6(A1),(A1)+		; mit zweitem Set tauschen		
	move.w	D1,4(A1)
	move.w	D1,(A0)+
	dbf	D0,3$
	movem.l	(SP)+,D0-D1/A0-A1
	rts

MouseXPos
	moveq	#0,D0
	move.w	spx,D0
return	rts

MouseYPos
	moveq	#0,D0
	move.w	spy,D0
	rts

x_VBLWait
	clr.w	vbl
1$	cmp.w	vbl,D0
	bgt.s	1$
	rts
;---------------------------------------------------------------------------
;
ResetCIAB
	move.b	#$7f,CB_ICR	; clear CIA B-IRQs
	move.b	CB_ICR,D0
	moveq	#0,D0
	move.b	D0,CB_CRB	; Timer Controll Register ruecksetzen
	move.b	D0,CB_CRA
	rts

;---------------------------------------------------------------------------
; **
; **  Interrupt Server, sowie die von diesen aufgerufenen Routinen
; **

; ** IRQ fuer Disk-Block-Done, wird ausgloest wenn DSKLEN NULL wird

DSKBLKdone
	move.w	#$0000,$DFF024	; DSKLEN ruecksetzen
	move.w	#$0002,$DFF09C	; IRQ-Request
	clr.b	flag
	rte

index_vec	DC.L	index0	; Vektor fuer IndexIRQ
I_enable	DC.B	$02,0	; Timer B IRQ immer erlauben
	EVEN

; ** CIA B IRQ-Routine, wird ausgeloest durch Timer A,B sowie das
; ** Index-Signal eines selektierten Laufwerks	

CIAB_server
	movem.l	D0-D1/A0,-(SP)

	move.b	CB_ICR,D1	; Server betreibt Polling, d.h. alle
	and.b	I_enable,D1	; anfordernden Quellen werden bearbeitet
	
	btst	#4,D1		; Indexbit testen
	beq.s	ti_a
	move.l	index_vec,A0	; Index IRQ aufgetreten
	jsr	(A0)	; 

ti_a	btst	#0,D1	
	beq.s	ti_b
	bsr	set_timer	; IRQ Timer A

ti_b	btst	#1,D1
	beq.s	3$		; IRQ Timer B (ca. 50 mal pro Sek)
	move.w	#$2000,$DFF09C	; andere IRQs zulassen, da hier sehr viel Zeit
	move.w	#$2000,SR	; verbraucht wird, auch ein rekursiver Aufruf
	bsr	TPS_Server	; ist moeglich

3$	move.w	#$2000,$DFF09C
	movem.l	(SP)+,D0-D1/A0
	rte	
 
index0	move.w	disklen,$DFF024	; sofort starten
	move.w	disklen,$DFF024
	btst	#6,disklen	; oberstes Byte von disklen!
	bne.s	writeonly	; => write Track
	move.l	#index1,index_vec ; => read Track
	rts
index1	move.l	#index2,index_vec
	rts
index2	clr.b	flag	; DMA-READY
	move.w	#$0000,$DFF024	; STOP-DMA, Short Track??
writeonly
	move.b	#$10,CB_ICR	; disable Index IRQ
	and.b	#$EF,I_enable	; FLG-Bit loeschen
	move.l	#index0,index_vec
	rts

; ** Keyboard IRQ-Server, empfaengt Keycode von der Tastatur und gibt 
; ** einen Handshake

port	move.l	D0,-(SP)
	move.b	CA_ICR,D0	; READ ICR, loescht CIA-IRQ Anforderung
	btst	#3,D0
	beq.s	.port_ret	; IRQ nicht von Keyboard ausgeloest
	;
	move.b	CA_SDR,D0	; Keycode auslesen
	move.b	#0,CA_SDR	; SDR = Serial Data Register
	move.b	#0,CA_SDR	; mit null loeschen
	ori.b	#$40,CA_CRA	; Handshake, SPMODE=OUT an CRA (Kdat=Low)
	eori.b	#$FF,D0		; Keycode invertieren
	lsr.b	#1,D0
	bcc.s	.p1
	ori.b	#$80,D0
.p1	move.b	D0,KeyBuf
	clr.w	KeyDelay	; Keydelay ruecksetzen, sodass neue Taste
	;			; sofort abgeholt werden kann
	move.w	#250,D0		; Kdat muss mindestens 74 us auf LOW liegen
	bsr	x_ShortDelay	; alle Register werden gerettet!!
	;
	andi.b	#$bf,CA_CRA	; und Haendeschuetteln, Bit 6 loeschen
	;
.port_ret
	move.w	#$0008,$DFF09C	; Interrupt Request loeschen
	move.l	(SP)+,D0
	rte
;---------------------------------------------------------------------------
;
; Syntax: StartTimer(delay in Mikrosekunden )
; Input :            D0.l	=>    0 us < delay < 917504 us
; Uses  : Input 
; Output: no  
; 
; Jeder TimerTick entspricht 1,4 us Mikrosekunden 
; (Taktfrequenz 7,1 MHZ durch 10, 1 Prozessortakt = 0,14 us )
; (PAL 7,09 MHZ; NTSC 7,16 MHZ)
;
; *** Note: Fuer sehr kleine Werte, lohnt sich kein Timer-Start, da schon
; ***       durch die Rechnerei (DIVU,MULU) die Zeit verbraucht wird!	
; ***       Loesung: direktes Auslesen von Timer B CIA B, Funktion x_ShortDelay!   

x_StartTimer
	bsr	timer_off
	cmp.l	#6553,D0	; D0 < 65536/10 ?
	ble.s	2$
	divu	#14,D0		; Mikrosekunden in Timerticks umrechnen
	mulu	#10,D0		; versagt wenn D0 < 14 ist
	divu	#$FFFF,D0	; oberes WORD Rest, unteres WORD Volldurchlaeufe 
	bra.s	3$
2$	mulu	#10,D0		; Ergebnis ist kleiner 65536
	divu	#14,D0
	swap	D0
	cmp.w	#7,D0		; 0 <= Rest <=13
	bls.s	4$		; rest ist kleiner als 7
	add.l	#$00010000,D0	; D0 aufrunden, indem 1 addiert wird
4$	clr.w	D0		; unteres WORD loeschen
3$	move.l	D0,timeval

	move.b	#$81,CB_ICR	; TA_IRQ erlauben
	or.b	#1,I_enable	; Bit in System-Maske setzen
	st	timer_flag	; Timer laeuft (= $FF)

set_timer:
	move.l	timeval,D0	; wird von Interrupt-Server gerufen
	tst.w	D0		; unteres WORD testen
	bne.s	.longval
	;
	swap	D0		; nur den Rest in den Timer schreiben
	tst.w	D0		; oberes WORD testen
	beq.s	timer_off	; wenn auch NULL, dann Stop
	clr.l	timeval		; erst beim naechsten Aufruf stoppen
	bra.s	.shortval
	;
.longval
	subq.l	#1,D0
	move.l	D0,timeval
	move.w	#$FFFF,D0	; einen vollen Timerdurchlauf starten	
	;
.shortval
	move.b	D0,CB_TALO	; LOW-Byte
	lsr.w	#8,D0
	move.b	D0,CB_TAHI	; HIGH-Byte, startet Timer schon
	ori.b	#$19,CB_CRA	; Load, Timer Start und One Shot
	rts

timer_off:
	andi.b	#$fe,CB_CRA	; Timer A CIA A stop
	and.b	#$FE,I_enable	; Bit in System-Maske loeschen
	move.b	#1,CB_ICR	; TA-IRQ verbieten
	sf	timer_flag	; Timer gestoppt (= 0)
	rts
;---------------------------------------------------------------------------
;
; Syntax: TestTimer <Sprungmarke fuer Schleife>
; Input : no       
; Output: no  

x_TestTimer
	tst.b	timer_flag	; Timer gestoppt ?
	rts
;---------------------------------------------------------------------------
;
; Syntax: Delay (delay in Mikrosekunden) ** vgl. StartTimer
; Input :        D0.l	     
; Output: no  

x_Delay:
	bsr	x_StartTimer
		
WaitTimer
	tst.b	timer_flag	; Timer gestoppt ?
	bne.s	WaitTimer
	rts
;---------------------------------------------------------------------------
;
; Syntax: ShortDelay (delay in Mikrosekunden)
; Input :                D0.b	  0 < D0.b	<= $FF Timerticks       
; Uses  : Input
; Output: no  
; ** Delay Routine, die mit dem kontinuierlich laufenden Timer B von CIA B
; ** arbeitet. Insbesondere fuer kleine Delay-Zeiten gegeignet,
; ** fuer die sich kein Aufruf von Starttimer lohnt.

x_ShortDelay
	movem.w	D1-D2,-(SP)	;  8+n*4 TZ
	and.w	#$00FF,D0	;  8 TZ
	clr.w	D2		;  4 TZ
	move.b	CB_TBLO,D2	; 16 TZ
1$	move.w	D2,D1		;  4 TZ
	sub.b	CB_TBLO,D1	; 16 TZ
	cmp.w	D0,D1		; 4 TZ
	blt.s	1$		; JUMP = 10 TZ, NOJUMP = 8 TZ 
	movem.w	(SP)+,D1-D2	; 12+n*4 TZ
	rts			; 16 TZ

	BSSSEG
timeval		DS.W	2
timer_flag	DS.W	1
	CODESEG
;---------------------------------------------------------------------------
;
; TPS (Ticks Per Second)
; Der Server wird ca. 50 mal pro Sekunde gerufen. Aufrufer ist der CIAB_Server, 
; Ausloeser ist ein IRQ von Timer B in CIA B. Der TPS_Server ersetzt den
; urspruenglichen Vertical_Blank Interrupt.
;

TPS_Server
	movem.l	D0-D7/A0-A6,-(A7) 

	lea	$DFF000,A6
	addq.w	#1,vbl		; Zeit-Zaehlvariablen
	addq.l	#1,SYSTime
	tst.w	KeyDelay	; es folgt die Dekrementierung 
	beq.s	1$		; verschiedener Delay-Variablen
	subq.w	#1,KeyDelay
	;
1$	tst.w	RMB_delay	; Right Mouse Button ca. 1 mal pro 
	beq.s	.rmb_test	; Sekunde abfragen.
	subq.w	#1,RMB_delay
	bra.s	.mou_test
.rmb_test	
	btst	#2,$dff016	; rechte Maustaste testen
	bne.s	.mou_test	; nicht gedrueckt
	eor.b	#1,mouseflag	; Flag umdrehen
	bsr	FlipMouseColors
	move.w	#TPS,RMB_delay	; Zeitverzoegerung ca. 1 Sek. einschalten
.mou_test
	tst.b	mouseflag
	bne.s	.no_mouse
	bsr.s	mouse		; kein Mausabfrage => Mouse freezed
.no_mouse
	tst.w	dir_active	; befindet sich PRG in Dir-Routine
	bne.s	.tps_end	; ja, keine Aufruf von Zeit oder Stopabfrage
	;
	tst.w	copy_active	
	beq.s	.tps_end	; Programm befindet sich schon
	;
	clr.w	copy_active	; in der Routine TestStop, oder COPY ist
	bsr	TestStop	; nicht aktiv
	bsr	TimeOut
	move.w	#1,copy_active
	;
.tps_end
	movem.l	(A7)+,D0-D7/A0-A6
	rts

; ** Subroutine fuer TPS-Server

 IFD NTMOUSE
mouse:	move.w	$dff00a,d0	;joy0dat
	move.w	d0,d2
	andi.w	#$00ff,d0
	move.w	d0,d1
	sub.w	oldx,d0
	move.w	d1,oldx
	ext.w	d0
	cmpi.w	#$007f,d0
	bmi.s	L4966
	move.w	#$00ff,d1
	sub.w	d0,d1
	move.w	d1,d0
	bra.s	L4970

L4966:	cmpi.w	#$ff81,d0
	bpl.s	L4970
	addi.w	#$00ff,d0
L4970:    
	add.w	d0,sp1x
	move.w	d2,d0
	lsr.w	#8,d0
	move.w	d0,d1
	sub.w	oldy,d0
	move.w	d1,oldy
	ext.w	d0
	cmpi.w	#$007f,d0
	bmi.s	L499a
	move.w	#$00ff,d1
	sub.w	d0,d1
	move.w	d1,d0
	bra.s	L49a4

L499a:	cmpi.w	#$ff81,d0
	bpl.s	L49a4
	addi.w	#$00ff,d0
L49a4:	add.w	d0,sp1y
	tst.w	sp1x
	bpl.s	L49b8
	clr.w	sp1x
L49b8:	cmpi.w	#1,sp1y
	bpl.s	L49ca
	move.w	#1,sp1y

L49ca:	cmpi.w	#$027f,sp1x
	bmi.s	L49dc
	move.w	#$027f,sp1x

L49dc:	cmpi.w	#$01ff,sp1y
	bmi.s	L49ee
	move.w	#$01ff,sp1y

L49ee:	moveq	#0,d0
	move.w	sp1x,d0
	lsr.w	#1,d0
	move.w	d0,spx
	moveq	#0,d0
	move.w	sp1y,d0
	lsr.w	#1,d0
	move.w	d0,spy
	bsr	Calc
	rts
 ENDC


mouse:
	move.w	$0A(A6),D4      ; JOYDAT0
	subq.b	#1,dely
	beq.s	.movmou
	move.b	D4,oldx
	lsr.w	#8,D4
	move.b	D4,oldy
	bra	.pop

.movmou	move.b	#DTIME,dely
	move.b	oldx,D0		; alte Counter Position X
	move.w	spx,D1		; aktuelle Sprite x Koor.
	moveq	#$7f,D2		; Maske
	move.w	D4,D3
	move.b	D3,oldx		; Counter X merken
	sub.b	D3,D0
	bpl.s	.left
;	eori.b	#$FF,D0
	neg.b	d0		;(H.V 21-9-91)
	and.w	D2,D0
	add.w	D0,D1		; move right
	cmpi.w	#318,D1		;318
	bmi.s	.sto1
	move.w	#318,D1
	bra.s	.sto1
.left	and.w	D2,D0
	sub.w	D0,D1
	bpl.s	.sto1
	moveq	#0,D1
.sto1	move.w	D1,spx		;neue xKoordinate

	move.b	oldy,D0		;alte Counter Y Position
	move.w	spy,D1
	move.w	D4,D3
	lsr.w	#8,D3		;vertical count
	move.b	D3,oldy		;neue Counter Y Position merken
	sub.b	D3,D0
	bpl.s	.up
;	eori.b	#$FF,D0		; Down
	neg.b	d0
	and.w	D2,D0
	add.w	D0,D1
	cmpi.w	#198,D1
	bmi.s	.sto2
	move.w	#198,D1
	bra.s	.sto2
.up	and.w	D2,D0		; Maske #$7f
	sub.w	D0,D1
	bpl.s	.sto2
	moveq	#0,D1
.sto2	move.w	D1,spy
	bsr.s	Calc		; aus spy,spx und sph CtrlLong errechnen
.pop	rts


;  ctrlong=Calc(xpos,ypos,height)  REGS: D0-D1/A0

Calc	moveq	#0,D1
	moveq	#0,D0
	move.w	spy,D0	; Vstart
	add.w	#$2C,D0 
	swap	D0
	lsl.l	#8,D0
	bcc.s	.cl1
	bset	#2,D1		; High VstartBit
.cl1	or.l	D0,D1
	;
	moveq	#0,D0
	move.w	spx,D0	; HStart
	lsr.w	#1,D0
	bcc.s	.cl2
	bset	#0,D1		; Low HStart Bit
.cl2	add.w	#64,D0
	swap	D0
	or.l	D0,D1
	;
	moveq	#0,D0		; Calculate VStop
	move.w	spy,D0
	add.w	#SPH+$2c,D0
;	add.w	#$2C,D0
	lsl.w	#8,D0
	bcc.s	1$
	bset	#1,D1		; High VSTop Bit
1$	or.l	D0,D1
	lea	sprite,A0	; CtrlLong speichern
	move.l	D1,(A0)
	rts

	BSSSEG
oldx	DS.W	1
oldy	DS.W	1
spx	DS.W	1
spy	DS.W	1
sp1x	DS.W	1
sp1y	DS.W	1
dely	DS.W	1
	CODESEG
;------------------------------------------------------------------
; Syntax: Ton(tonart)
; Input :     D0.b	= 0 OK-Ton, anderenfalls Ton fuer einen Error
; Uses  : Input
; Output: no
	
x_Ton:	movem.l	D1-D7,-(A7)
	;
.ok	moveq	#0,D4
	tst.b	D0
	bne.s	.tonbad
	move.w	#$F00,D4
	move.l	D4,D5
	sub.w	#$200,D5
	bra.s	.tx
	;
.tonbad move.w	#$7700,D4
	move.l	D4,D5
	sub.w	#$200,D5
	;
.tx	move.l	#SOUNDDATA,$D0(A6)
	move.w	#$0002,$D4(A6)
	move.w	#$3f,$D8(A6)
	move.w	#$8008,$96(A6)
	;
	moveq	#64,D6
.loop	move.w	D6,$D8(A6)
	move.l	D4,d3
	move.l	D5,d2
	bsr.s	.down
	move.l	D4,d3
	move.l	D5,d2
	bsr.s	.up1
	subq.l	#8,D6	
	bpl.s	.loop
	;
	moveq	#0,d0
	move.w	d0,$D8(A6)
	move.w	d0,$d4(a6)
	move.w	#$0008,$96(A6)
	movem.l	(A7)+,D1-D7
	rts

.down	move.w	d3,$D6(A6)
	moveq	#$40,D0
	bsr	x_ShortDelay
	subq.l	#1,d3
	cmp.l	d2,d3
	bgt.s	.down
	rts

.up1	move.w	d2,$D6(A6)
	moveq	#$40,D0
	bsr	x_ShortDelay
	addq.l	#1,d2
	cmp.l	d2,d3
	bne.s	.up1
	rts

;------------------------------------------------------------------------------
;
;                              GFX Library
;
; ** Version: 1.00
; ** Date   : 24/9/90
; ** Autor  : Frank Neuhaus
; 
; ** Die Library enthaelt alle Routinen die auf den Blitter zugreifen
; ** sowie Fenster- und Menueverwaltungsroutinen. Die Aufrufkonvention der
; ** einzelnen Routinen sind jeweils im Programmtext dokumentiert sowie 
; ** im INCLUDE-File gfxlib.i. Allgemein gilt, dass nur die Register veraendert
; ** werden, die die Aufrufparameter enthalten. Alle anderen Register sind  
; ** nach dem Aufruf unveraendert.
; 
;	xdef x_RectFill
;	xdef x_HighLight
;	xdef x_Box
;	xdef x_Plot
;	xdef x_Line
;	xdef x_SaveWindow
;	xdef x_RestoreWindow
;	xdef x_OpenWindow
;	xdef x_CheckGadgets
;	xdef x_Print

dmaconr = $02
vhposr  = $06
con0    = $40
con1    = $42
afwm    = $44
alwm    = $46
	
cpth    = $48
bpth    = $4C
apth    = $50
dpth    = $54
size    = $58
cmod    = $60
bmod    = $62
amod    = $64
dmod    = $66
cdat    = $70
bdat    = $72
adat    = $74

;---------------------------------------------------------------------------
;
; Syntax: RectFill(X1,  Y1,  X2,  Y2,  color)
; Input :          D0.w	D1.w	D2.w	D3.w	D4.b
; Uses  : InputRegs
; Output: no  

x_RectFill:
	st blitter	; Blitter sperren = $FF
	movem.l	A0-A1/D5-D7,-(SP)

	bsr	x_recsetup

	moveq	#DEP-1,D0
x_reclop
	move.w	#$0B0A,D1	; loeschen
	lsr.b	#1,D4
	bcc.s	1$
	move.w	#$0BFA,D1	; setzen
1$	move.w	D1,con0(A6)	; D = A 
	move.l	#CHARBUF,apth(A6)	; LEERZEILE
	move.l	A0,cpth(A6)
	move.l	A0,dpth(A6)
	move.w	D3,size(A6)
	adda.w	#BPR,A0		; naechste Plane beginnt eine Zeile tiefer
	bsr	waitblit
	dbf	D0,x_reclop
	sf	blitter
	movem.l	(SP)+,A0-A1/D5-D7
	rts

x_HighLight:
	st	blitter			; Blitter sperren = $FF
	movem.l	A0-A1/D4-D7,-(SP)
	bsr	x_recsetup
	move.w	#$0b5A,con0(A6)		; D = (!A and C) or (A and !C)
	moveq	#DEP-1,D0
1$	move.l	#CHARBUF,apth(A6)	; A = D
	move.l	A0,cpth(A6)
	move.l	A0,dpth(A6)
	move.w	D3,size(A6)
	adda.w	#BPR,A0
	bsr	waitblit
 IFEQ SOLO
	dbf	D0,1$
 ENDC
	sf	blitter
	movem.l	(SP)+,A0-A1/D4-D7
	rts


; ** x_recsetup berechnet die wichtigsten Parameter fuer RecFill und Highlight
; ** GFX internal use only!

x_recsetup
	move.l	#CHARBUF,A0	; Maske erstellen
	moveq	#9,D5		; 10 Langworte
1$	move.l	#-1,(A0)+
	dbf	D5,1$

	bsr	x_RecMinMax
	bsr	waitblit

	move.w	D0,D6		; X1
	and.w	#$0F,D6		; untere 16 Bits ausmaskieren
	move.l	#-1,D5		; Maske
	lsr.w	D6,D5		; in Position schieben
	move.w	D5,afwm(A6)	; First Word Mask

	move.w	D2,D6		; X2
	and.w	#$0F,D6
	addq.w	#1,D6
	clr.w	D5		; Maske vom oberen WORD in D5 
	lsr.l	D6,D5		; ins untere schieben
	move.w	D5,alwm(A6)	; Last Word Mask

; Blit-Size in D3 (= Y2) berechnen

	sub.w	D1,D3		; Y2-Y1, Y2 muss groesser als Y1 sein !!
	addq.w	#1,D3		; Hoehe+1
	lsl.w	#6,D3		; 6 Bits nach links (vertikaler Zaehler)
	lsr.w	#4,D0		; X1 / 16, D0 = Hor. Startpos in WORDS
	lsr.w	#4,D2		; X2 / 16
	sub.w	D0,D2		; horizontale Breite errechnen
	addq.w	#1,D2		; D2 enthaelt nun die Breite in WORDS
 	or.w	D2,D3		; D3 = Size
	lsl.w	#1,D2		; nun hat D2 die Breite in Bytes = MODULO A

	move.l	planeptr,A0	; in welche MAP WIRD gezeichnet ?!
	mulu	#BPR*DEP,D1	; Pointer in Bitmap berechen
	adda.l	D1,A0
	adda.w	D0,A0		; 2 mal D0 addieren,da Byteoffset gesucht
	adda.w	D0,A0

	move.w	#BPR*DEP,D5	; MODULO fuer D errechnen
	sub.w	D2,D5
	neg.w	D2		; negativer MODULO
	move.w	D2,amod(A6)
	move.w	D5,cmod(A6)
	move.w	D5,dmod(A6)  
	move.w	#0,con1(a6)
	rts

; D0 = MIN(X1,X2), D1 = MIN(Y1,Y2) 
;          D0 D2            D1 D3
; ** GFX internal use only!

x_RecMinMax
	cmp.w	D0,D2	; X1,X2
	bhi.s	1$	; JA
	exg	D0,D2
1$	cmp.w	D1,D3	; Y1<Y2
	bhi.s	2$	; JA
	exg	D1,D3
2$	rts

;---------------------------------------------------------------------------
;
; Syntax: Plot(X1,  Y1,  color)
; Input :      D0.w	D1.w	D2.b
; Uses  : Input
; Output: no  

x_Plot:
	move.l	A0,-(SP)
	move.l	planeptr,A0
	mulu	#DEP*BPR,D1
	add.l	D1,A0
	move.w	D0,D1	; X
	lsr.w	#3,D1	; X/8
	adda.w	D1,A0	
	moveq	#7,D1
	and.w	D1,D0	; X MOD 8
	eor.w	D1,D0	; invertieren, da umgekehrte Zaehlweise
	moveq	#DEP-1,D1
x_plolop
	lsr.b	#1,D2
	bcc.s	p_clr
	bset.b	D0,(A0)
	bra.s	n_pla
p_clr	bclr.b	D0,(A0)
n_pla	adda.w	#BPR,A0
	dbf	D1,x_plolop
	move.l	(SP)+,A0
	rts

;---------------------------------------------------------------------------
;
; Syntax: Box(X1,  Y1,  X2,  Y2,  color)
; Input :     D0.w	D1.w	D2.w	D3.w	D4.b
; Uses  : InputRegs 
; Output: no  

x_Box	bsr	x_RecMinMax
	movem.w	D0-D4,-(SP)
	move.w	D1,D3		; Y2 = Y1
	bsr	x_Line
	movem.w	(SP),D0-D4
	move.w	D2,D0		; X1 = X2
	bsr	x_Line
	movem.w	(SP),D0-D4
	move.w	D3,D1		; Y1 = Y2
	bsr	x_Line
	movem.w	(SP)+,D0-D4
	move.w	D0,D2		; X2 = X1
	bra	x_Line

;---------------------------------------------------------------------------
;
; Syntax: Line(X1,  Y1,  X2,  Y2,  color)
; Input :      D0.w	D1.w	D2.w	D3.w	D4.b
; Uses  : InputRegs
; Output: no  

okta	DC.B	(0<<2)+1,(4<<2)+1,(2<<2)+1,(5<<2)+1
	DC.B	(1<<2)+1,(6<<2)+1,(3<<2)+1,(7<<2)+1

x_Line	movem.w	D0-D2,-(SP)	; Anfangs- und Endpunkt mit Plot setzen
	move.w	D4,D2
	bsr	x_Plot
	movem.w	(SP),D0-D2
	move.w	D2,D0
	move.w	D3,D1
	move.w	D4,D2
	bsr	x_Plot
	movem.w	(SP)+,D0-D2

	movem.l	D5/A0,-(SP)
	st	blitter	; Blitter sperren = $FF

	move.l	planeptr,A0
	move.w	#BPR*DEP,D5
	mulu	D1,D5
	adda.l	D5,A0
	move.w	D0,D5
	lsr.w	#4,D5	; (X/8) MOD 16
	lsl.w	#1,D5
	adda.w	D5,A0

	moveq	#0,D5
	sub.w	D1,D3	; DeltaY in D3
	roxl.b	#1,D5
	tst.w	D3
	bge.s	y2gy1
	neg.w	D3
y2gy1	sub.w	D0,D2	; DeltaX in D2
	roxl.b	#1,D5
	tst.w	D2
	bge.s	x2gx1
	neg.w	D2
x2gx1	move.w	D3,D1
	sub.w	D2,D1
	bge.s	dygdx
	exg	D2,D3
dygdx	roxl.b	#1,D5
	tst.w	D2
	bne.s	2$
	tst.w	D3
	bne.s	2$		; beide Deltas sind NULL!!
	bra	return
2$	move.b	okta(PC,D5.W),D5
	add.w	D2,D2		; 2*KDelta

	bsr	waitblit
	move.w	D2,bmod(A6)
	sub.w	D3,D2		; 2*Kdelta-GDelta
	bge.s	1$
	or.b	#$40,D5		; Sign-Flag in BLTCON1 setzen
1$	move.w	D2,apth+2(A6)
	sub.w	D3,D2		; 2*KDelta-GDelta
	move.w	D2,amod(A6)
	move.w	#BPR*DEP,cmod(A6)
	move.w	#BPR*DEP,dmod(A6)

	and.w	#$000f,D0	; CON0 und CON1 errechnen
	ror.w	#4,D0
;	or.w	D0,D5 
	move.w	D5,con1(A6)
	or.w	#$0BCA,D0

	lsl.w	#6,D3		; Size errechnen
	addq.w	#2,D3

	move.l	#$FFFFFFFF,afwm(A6)
	move.w	#$8000,adat(A6)

	moveq	#DEP-1,D2
x_linlop
	moveq	#0,D1
	lsr.b	#1,D4
	bcc.s	6$
	move.w	#$ffff,D1
6$	move.w	D1,bdat(A6)	; Texture
	move.w	D0,con0(A6)
	move.l	A0,cpth(A6)
	move.l	A0,dpth(A6)
	move.w	D3,size(A6)
	bsr	waitblit
	adda.w	#BPR,A0
	dbf	D2,x_linlop
	sf	blitter		; Blitter freigeben
	movem.l	(SP)+,D5/A0
	rts
;---------------------------------------------------------------------------
;  
; Syntax: SaveWindow(X1,  Y1,  X2,  Y2,)
; Input :            D0.w	D1.w	D2.w	D3.w
; Uses  : InputRegs
; Output: D0.l	= Zeiger auf Pufferspeicher

x_SaveWindow
	movem.l	D5/A0-A1,-(SP)
	move.w	#BPR*DEP,D5
	mulu	D1,D5		; Y1*BPR*DEP 
	move.l	planeptr,A0
	add.l	D5,A0
	sub.w	D1,D3		; DeltaY in D3
	addq.w	#1,D3		; Anzahl der Zeilen
	mulu	#DEP,D3		; mal Anzahl der Planes
	lsr.w	#4,D0		; X1/16 => WORDS
	lsr.w	#4,D2		; X2/16
	addq.w	#1,D2
	sub.w	D0,D2		; DeltaX in WORDS
	lsl.w	#1,D0
	adda.w	D0,A0		; X-Offset in BitMap addieren
	move.w	D2,D0
	lsl.w	#1,D0		; D0 nun BytesPerRow
	mulu	D3,D0		; mal Zeilen = Speicherplatz
	addq.l	#8,D0		; Raum fuer Headerinfo
	addq.l	#4,D0
	move.l	D0,D5

	moveq.l	#0,D1		; PUBLIC
	bsr	AllocMem	; sichert alle Register bis auf D0!!
	tst.l	D0		; D0 is Funktionswert von x_SaveWindow
	beq	Bye
	move.l	D0,A1		; Zeiger auf Speicherbereich
	move.l	A0,(A1)+	; Zeiger in Bitmap zwischenspeichern
	move.w	D3,(A1)+	; Deltay zwischenspeichern
	move.w	D2,(A1)+	; Deltax zwischenspeichern
	move.l	D5,(A1)+	; Groesse des Memory-Blocks

	move.w	#BPR,D5		; Modulo berechnen
	move.w	D2,D1
	lsl.w	#1,D1
	sub.w	D1,D5

	subq.w	#1,D2
	subq.w	#1,D3
1$	move.w	D2,D1		; DeltaX in WORDS
2$	move.w	(A0)+,(A1)+	; Grafikdaten retten
	dbf	D1,2$
	adda.w	D5,A0
	dbf	D3,1$		; DeltaY-1
	movem.l	(SP)+,D5/A0-A1
	rts	
;---------------------------------------------------------------------------
;  
; Syntax: RestoreWindow(Zeiger auf Pufferspeicher)
; Input :               A0
; Uses  : no
; Output: no

x_RestoreWindow
	movem.l	D0-D4/A1-A2,-(SP)
	move.l	A0,A2		; Zeiger auf Mem-Block
	move.l	(A2)+,A1	; Zeiger auf Bitmap nach A1
	move.w	(A2)+,D2	; Deltay
	move.w	(A2)+,D1	; Deltax
	move.l	(A2)+,D0	; Groesse des Memory-Blocks => Freemem!!

	move.w	#BPR,D4		; Modulo berechnen
	move.w	D1,D3
	lsl.w	#1,D3
	sub.w	D3,D4

	subq.w	#1,D1
	subq.w	#1,D2
1$	move.w	D1,D3
2$	move.w	(A2)+,(A1)+	; Restore-Schleife
	dbf	D3,2$
	adda.w	D4,A1
	dbf	D2,1$
	move.l	A0,A1		; Zeiger auf Mem-Block, D0 = Size
	bsr	FreeMem
 	movem.l	(SP)+,D0-D4/A1-A2
	rts

;---------------------------------------------------------------------------
;  
; Syntax: OpenWindow(Zeiger auf Window-Structure)
; Input :               A0
; Uses  : no
; Output: D0.l	= Zeiger auf Pufferspeicher

x_OpenWindow:
	movem.l	D1-D5/A0-A2,-(SP)
	move.l	A0,A2			; Windowpointer nach A2
	movem.w	WIN_X1(A2),D0-D3	; X1,Y1,X2,Y2 laden
	bsr	x_SaveWindow
	move.l	D0,-(SP)		; Zeiger auf Puffer retten

	movem.w	WIN_X1(A2),D0-D3	; X1,Y1,X2,Y2 laden
	move.b	WIN_COL(A2),D4		; Hintergrundfarbe laden
	bsr	x_RectFill

	movem.w	WIN_X1(A2),D0-D3	; X1,Y1,X2,Y2 laden
	move.b	WIN_BCOL(A2),D4		; Rahmenfarbe laden 
	bsr	x_Box			; Fenster umrahmen	
	SetTextMode SHADOW!BACKGND

	tst.l	WIN_STR(A2)
	beq.s	x_wibox			; Window besitzt keine String-Gadgets

	move.l	WIN_STR(A2),A0		; zeigt auf GadgetAnzahl
	move.w	(A0)+,D5		; D5 = Counter
	move.l	A0,A1			; Pointer auf String-Gadget Start merken

x_wigad	add.w	(A0)+,A1	; Laenge der Struktur zu Base addieren
	movem.w	(A0)+,D0-D1	; X1/Y1 holen
	move.b	(A0)+,D2	; Farbe holen
	tst.b	(A0)+		; Rueckgabewert ueberlesen,A0 => zeigt auf Text
	bsr	x_Print		; A0 zeigt danach auf naechstes Gadget
	move.l	A1,A0		; Pointer auf naechstes Gadget
	dbf	D5,x_wigad

x_wibox	tst.l	WIN_BOX(A2)
	beq.s	x_witxt		; Window besitzt keine Box-Gadgets	

	move.l	WIN_BOX(A2),A0	; zeigt auf GadgetAnzahl
	move.w	(A0)+,D5	; D5 = Counter
	move.l	A0,A1		; Pointer auf Box-Gadget Start merken

x_wibox1
	add.w	(A0)+,A1
	movem.w	(A0)+,D0-D3	; X1,Y1,X2,Y2
	move.b	(A0)+,D4	; Boxfarbe
	tst.b	(A0)+		; RET-Parm ueberlesen
	movem.w	D0-D1,-(SP)	; X/Y merken
	bsr	x_Box
	movem.w	(SP)+,D0-D1	; X/Y fuer String
	addq.w	#2,D0
	addq.w	#2,D1
	move.b	(A0)+,D2	; String Farbe
	bsr	x_Print
	move.l	A1,A0
	dbf	D5,x_wibox1

x_witxt	tst.l	WIN_TXT(A2)
	beq.s	x_wiret		; Window besitzt keine reinen Texte	

	move.l	WIN_TXT(A2),A0	; zeigt auf GadgetAnzahl
	move.w	(A0)+,D5	; D5 = Counter
	move.l	A0,A1		; Pointer auf Box-Gadget Start merken

x_witxt1
	add.w	(A0)+,A1	; Size addieren
	moveq	#0,D0
	move.b	TXT_FLG(A0),D0	; DrawFlags holen
	bsr	x_SetTextMode	; und setzen
	movem.w	(A0)+,D0-D1	; X1,Y1
	move.b	(A0)+,D2	; Textfarbe
	tst.b	(A0)+		; DrawFlags uberlesen
	bsr	x_Print
	move.l	A1,A0
	dbf	D5,x_witxt1

x_wiret	SetTextMode NOSHADOW!NOBACKGND
	move.l	(SP)+,D0	; Zeiger auf Puffer von x_SaveWindow holen
	movem.l	(SP)+,D1-D5/A0-A2
	rts
;---------------------------------------------------------------------------
;  
; Syntax: CheckGadgets(Zeiger auf Window-Structure)
; Input :               A0
; Uses  : no
; Output: no

	BSSSEG
regtmp	DS.W	4
ret_par	DS.B	2
	CODESEG

x_CheckGadgets:
	movem.l	D1-D7/A0/A2,-(SP)	
	move.l	A0,A2	; Zeiger auf WindowStructure nach A2
	st	ret_par	; ret_par = $FF
	
endless	bsr	MouseYPos	; aktuelle Mausposition nach D5/D6
	move.w	D0,D6
	bsr	MouseXPos
	move.w	D0,D5

	tst.l	WIN_STR(A2)	; hat Window String-Gadgets
	beq	no_gads	; nein

; ** alle String-Gadgets ueberpruefen, diese liegen relativ zum WindowStart

	movem.w	WIN_X1(A2),D0-D3  ; X1,Y1,X2,Y2 laden
	move.l	WIN_STR(A2),A0
	move.w	(A0)+,D7	; Anzahl der Gadgets

	moveq	#1,D4	; Loeschflag

g_loop	move.w	GAD_Y(A0),D1	; neues Y1 aus Gadget holen
	move.w	D1,D3
	addq.w	#7,D3	; Y2 = Y1+7

	cmp.w	D0,D5	; mousex < X1
	blt.s	next_gad
	cmp.w	D1,D6	; mouesy < Y1
	blt.s	next_gad
	cmp.w	D2,D5
	bgt.s	next_gad
	cmp.w	D3,D6
	bgt.s	next_gad

	clr.w	D4		; Maus steht auf Gadget
	swap	D1		; oberes WORD ist frei
	move.b	GAD_RET(A0),D1
	cmp.b	ret_par,D1
	beq.s	next_gad	; Maus steht auf altem Gadget 
	swap	D1		; D1 wieder herstellen
	addq.w	#1,D0		; neues Gadget wurde angeklickt
;	addq.w	#1,D1
	subq.w	#1,D2
;	subq.w	#1,D3
	bsr	deselect	; ev. altes Gadget desektieren	
	movem.w	D0-D3,regtmp	; Werte des neuen Gadgets speichern
	bsr	x_HighLight
	move.b	GAD_RET(A0),ret_par	; Rueckgabewert holen
	VBLWait 2

next_gad
	adda.w	GAD_LEN(A0),A0	; Laenge des Gadgets, Zeiger auf naechstes
	dbf	D7,g_loop
	tst.w	D4
	beq.s	prell
	bsr	deselect
	st	ret_par	; ret_par = $FF
prell
; ** hier Warteschleife einfuegen	

no_gads	btst	#6,$BFE001	; auf erste Maustaste warten
	bne	endless		; linke Maustaste nicht gedrueckt  	
	cmp.b	#$FF,ret_par
	bne	gad_click	; es ist ein STR_GADGET angeclickt worden

	tst.l	WIN_BOX(A2)	; MausClick war ausserhalb
	beq	no_gads		; aller Clickbereiche

; ** alle Box-Gadgets ueberpruefen, diese liegen absolut zum Window

	move.l	WIN_BOX(A2),A0
	move.w	(A0)+,D7	; Anzahl der Box-Gadgets
	move.l	A0,A1

x_ckbox	movem.w	BOX_X1(A0),D0-D3	; X1,Y1,X2,Y2 holen	
	cmp.w	D0,D5			; mousex < X1
	blt.s	next_box
	cmp.w	D1,D6			; mouesy < Y1
	blt.s	next_box
	cmp.w	D2,D5
	bgt.s	next_box
	cmp.w	D3,D6
	bgt.s	next_box

	addq.w	#1,D0
	addq.w	#1,D1
	subq.w	#1,D2
	subq.w	#1,D3
	movem.w	D0-D3,regtmp	; Werte des neuen Gadgets speichern
	bsr	x_HighLight
	move.b	BOX_RET(A0),ret_par ; Rueckgabewert holen
	VBLWait 2
	bra	gad_click

next_box
	adda.w	BOX_LEN(A0),A0
	dbf	D7,x_ckbox
	bra	endless

gad_click
;	VBLWait 8
	bsr	deselect
;	VBLWait 8
	moveq	#0,D0
	move.b	ret_par,D0
	movem.l	(SP)+,D1-D7/A0/A2
	rts

; ** x_CheckGadgets internal use only 

deselect
	cmp.b	#$FF,ret_par
	beq.s	.noold
	movem.w	D0-D3,-(SP)
	movem.w	regtmp,D0-D3
	bsr	x_HighLight	; altes Gadget wieder normal darstellen
	movem.w	(SP)+,D0-D3
.noold	rts

;---------------------------------------------------------------------------
;  
; Syntax: SetTextMode(Flags)
; Input :              D0.w	
; Use   : InputRegs
; Output: no

textflags	DC.W	0	; NOSHADOW or NOBACKGND

x_SetTextMode
	move.l	a0,-(sp)
	lea	textflags(pc),a0
	move.w	#BPR*DEP,ZEILE
	move.w	#BPR,BREITE
	move.l	planeptr,DRAWMAP
	move.w	#DEP-1,DEPTH
	move.w	D1,-(SP)
	btst	#7,D0
	bne.s	clearbit
	or.w	D0,(a0)
	bra.s	setpar
clearbit	
	not	D0
	and.w	D0,(a0)
setpar	move.w	(a0),D0
	move.w	D0,D1
	move.l	#CHARMASK,MASK
	and.w	#BACKGND,D1
	beq.s	1$	; nicht gesetzt
	move.l	#CHARBUF,MASK	; Background nicht loeschen
1$	move.w	(SP)+,D1
	move.l	(sp)+,a0
	rts
;---------------------------------------------------------------------------
;  
; Syntax: Print(Zeiger auf String, X,   Y,  Color)
; Input :              A0         D0.w	D1.w D2.w	
; Use   : InputRegs
; Output: A0 => zeigt auf 1.tes Byte hinter Eingabestring

x_Print:
	movem.l	D3-D4/A1-A3,-(SP)
	bsr	text2bits	; erstellt Text-Bitplane, Modulo danach in D4
	move.w	textflags,D3	; A0 zeigt auf Stringende
	and.w	#SHADOW,D3
	beq.s	noshadow
	movem.w	D0-D2/D4,-(SP)
	addq.w	#1,D0	; Text um +1,+1 nach unten versetzen
	addq.w	#1,D1
	moveq	#0,D2	; Farbe schwarz		
	bsr	x_printin
	movem.w	(SP)+,D0-D2/D4
noshadow
	bsr	x_printin
	movem.l	(SP)+,D3-D4/A1-A3
	rts
; ** x_Print internal use only

x_printin
	move.l	DRAWMAP,A1	; in welche wird MAP gezeichnet ?!
	mulu	ZEILE,D1	; Pointer in Bitmap berechen
	add.l	D1,A1

	bsr	waitblit
	st	blitter		; Blitter sperren = $FF
	move.l	#$FFFF0000,afwm(A6)
	move.w	#-2,amod(A6)
	move.w	#-2,bmod(A6)
	addq.w	#2,D4		; Modulo+2
	move.w	ZEILE,D1
	sub.w	D4,D1
	move.w	D1,cmod(A6)
	move.w	D1,dmod(A6)
	move.w	#(8<<6),D1	; SIZE
	lsr.w	#1,D4
	or.w	D4,D1
	move.w	D1,D4

blitagain
	move.w	D0,D1
	lsr.w	#4,D1
	lsl.w	#1,D1
	add.w	D1,A1
	swap	D0	; Shift berechnen
	clr.w	D0
	lsr.l	#4,D0
	move.w	D0,con1(A6)
	or.w	#$0FCA,D0	   
	move.w	D0,con0(A6)

	move.w	DEPTH,D0	; X mal loop durchlaufen
1$	lsr.b	#1,D2
	bcs.s	2$
	move.l	#CHARMASK+320,bpth(A6)	; LEERZEILE
	bra.s	3$
2$	move.l	#CHARBUF,bpth(A6)
3$	move.l	MASK,apth(A6)
	move.l	A1,cpth(A6)
	move.l	A1,dpth(A6)
	move.w	D4,size(A6)
	bsr	waitblit
	add.w	BREITE,A1
	dbf	D0,1$
	sf	blitter	; Blitter freigeben = $00
	rts

text2bits:
	move.l	A0,A2	; A0 Zeiger auf Text
	move.l	#-1,D4
txlen	addq.l	#1,D4
	tst.b	(A2)+	; Endlosschleife wenn keine NULL!!!
	bne.s	txlen
	lea	CHARBUF,A2	
	lea	CHARMASK,A3	
	btst	#0,D4	; Laenge aufrunden
	beq.s	charout
	addq.w	#1,D4
	moveq	#$07,D3	; 1 Leerzeichen an Maske/Source
	movem.l	A2-A3,-(A7)
2$	add.w	D4,A2	; naechste Zeile der Source
	add.w	D4,A3	; naechste Zeile der Destination
	sf	-1(A2)	; FALSE=$00
	sf	-1(A3)
	dbf	D3,2$
	movem.l	(A7)+,A2-A3	; kann auch durch MASK

charout	moveq	#0,D3
	move.b	(A0)+,D3	; Zeichen aus String holen
	beq.s	return2	; Ende
	cmp.w	#32,D3	; auf ungueltige Character ueberpruefen
      	blt.s	illchar	; (d.h. nicht im Charset)
       	cmp.w	#126,D3
       	bgt.s	illchar
       	sub.w	#32,D3	 ; A0 enthaelt Pointer auf Zeichen
space   	
	lea	charset,A1
       	cmp.w	#40,D3
       	blt.s	1$
       	add.w	#8*40,A1	; nur 7 mal, da D3 addiert wird
	sub.w	#40,D3
       	cmp.w	#40,D3
       	blt.s	1$
       	add.w	#7*40,A1
1$ 	add.w	D3,A1

	moveq	#$07,D3
	movem.l	A2-A3,-(A7)
pt5	move.b	(A1),(A2)
	st	(A3)		; TRUE=$FF 
	add.w	#40,A1		; naechste Zeile der Source
	add.w	D4,A2		; naechste Zeile der Destination
	add.w	D4,A3		; naechste Zeile der Maske
	dbf	D3,pt5
	movem.l	(A7)+,A2-A3
	addq.w	#1,A2
	addq.w	#1,A3
	bra.s	charout
	;
illchar	moveq	#0,D3		; Leerzeichen ausgeben
	bra.s	space
return2	rts

waitblit
	btst	#6,$02(A6)
	btst	#6,dmaconr(A6)
	bne.s	1$
	rts
1$	nop
	nop
	btst	#6,$2(A6)
	bne.s	1$
	rts

; ** Ende der erneuerten Routinen der GFXLIB
; ** es folgen Teile der alten Routinen

;--------------------------------------------------------------------------
;
;	Zeichnen eines BOBS ohne Clipping
;
;	REGISTER A0-A1/D0-D2	PARMS: A0 Shapepointer, D0/D1 X/Y
;
Flash:	lsl.w	#2,D2		; *4
	lea	Icons,A0
	move.l	(A0,D2.W),A0	; Iconpointer
	;
	move.w	#$0FCA,MINTERM
	movem.l	D0-D1/A0,-(A7)
	bsr	DrawBob
	VBLWait 8
	movem.l	(A7)+,D0-D1/A0
	move.w	#$0B0A,MINTERM
	bsr	DrawBob
	move.w	#$0FCA,MINTERM
	rts

; D0=X, D1=Y, D2=IconNR

DrawMap:
	lsl.w	#2,D2		; IconNR*4
	lea	Icons,A0
	move.l	(A0,D2.W),A0	; Iconpointer
	move.w	#$0FCA,MINTERM	

 IFNE SOLO
	cmp.w	#2*4,d2
	bgt.s	DrawBob
	;
	move.w	#%0000000100000000,MINTERM	;clear destination
	movem.l	d0-d1/a0,-(sp)
	bsr.s	DrawBob
	movem.l	(sp)+,d0-d1/a0
	move.w	#$0FCA,MINTERM	
 ENDC
	;
DrawBob:			; A0=Pointer auf Shape-Structure,D0=X,D1=Y
	st	blitter		; Blitter sperren = $FF
	move.l	planeptr,A1	; in welche wird MAP gezeichnet ?!
	mulu	#(BPR*DEP),D1	; Pointer in Bitmap berechen
	add.w	D1,A1
	;
	move.w	D0,D1
	lsr.w	#4,D1
	lsl.w	#1,D1
	add.w	D1,A1
	swap	D0		; Shift berechnen
	clr.w	D0
	lsr.l	#4,D0		; D0 enthaelt nun SHIFT
	;
	moveq	#BPR/2-1,D2	; Destination Modulo berechen
	sub.w	(A0),D2		; Breite in Words abziehen
	lsl.w	#1,D2		; Modulo in Bytes
	;
	move.w	2(A0),D1	; Size kalkulieren
	mulu	#DEP*64,D1	; DEP Planes/ 4*64=256 <=> lsl #8,D1
	add.w	(A0),D1		; Breite in Worten
	addq.w	#1,D1
	;
	bsr	waitblit
	move.l	#$FFFF0000,afwm(A6)
	move.w	#-2,bmod(A6)
	move.w	#-2,amod(A6)
	move.w	D0,con1(A6)
	or.w	MINTERM,D0        
	move.w	D0,con0(A6)
	move.w	D2,cmod(A6)
	move.w	D2,dmod(A6)
	move.l	A1,cpth(A6)
	move.l	A1,dpth(A6)
	move.l	4(A0),apth(A6)	; A = Maske
	lea	8(A0),A1
	move.l	A1,bpth(A6)	; Source nach B
	move.w	D1,size(A6)
	bsr	waitblit
	sf	blitter		; Blitter freigeben = $00
	rts


;	*** TimeOut
;	*** -------
tbuf	DC.L	0,0
xkor	DC.W	279,285,294,300
tfor	DC.B	"%04ld",0
	EVEN
	;
TimeOut:
	tst.w	TDly
	beq.s	2$
	subq.w	#1,TDly
	rts
	;
2$	btst	#6,dmaconr(A6)	; WIRD NUR von  VBL gerufen !!!
	bne	return		; Blitter BUSY, dann zurueck
	tst.b	blitter		; Blitter gesperrt
	bne	return
	move.w	#TPS,TDly
	moveq	#0,D0
	move.l	SYSTime,D0
	divu	#TPS,D0		; 50 HZ = Anzahl der Sekunden
	swap	D0
	clr.w	D0		; Rest loeschen
	swap	D0
	divu	#60,D0		;  Sek/60 = Minuten
	move.w	D0,D1
	cmp.w	#60,D1		; mehr als 60 Minuten?
	blt.s	3$		; nach einer Stunde reiner COPYZEIT
	clr.l	SYSTime		; die Zeit auf 00:00 stellen
	moveq	#0,D0
	moveq	#0,D1
3$	mulu	#100,D1		; Minuten mal 100
	swap	D0
	add.w	D0,D1		; Sekunden addieren	
	
	move.l	D1,-(A7)	; Zeit auf Stack
	XPRINTF tbuf,tfor,4

	lea	tbuf,A2
	lea	xkor,A3

	moveq	#3,D7		; 4 Zahlen ausgeben
1$	move.l	planeptr,A1
	add.l	#175*BPR*DEP,A1	; Y-Offset
	moveq	#0,D3
	move.b	(A2)+,D3
	sub.b	#$30,D3	; 0-9
	moveq	#LGREY,D4
	move.w	(A3)+,D1	; X-Koordinate
	bsr	miniout
	dbf	D7,1$
notim	rts

Versionnr:
	move.l	planeptr,A1
	add.l	#BPR*DEP*2,A1	; Y=2
	moveq	#LGREY,D4
	movem.l	D4/A1,-(SP)
	moveq	#3,D3		; Version 3.
	move.w	#254,D1		; X fuer erste Zahl
	bsr	miniout
	movem.l	(SP)+,D4/A1
	moveq	#$c,D3		; Version 3.3
	move.w	#261,D1		; X fuer 2.te Zahl
	bra	miniout

Display:
	tst.w	dir_active
	bne	nodisp
	move.l	planeptr,A1
	moveq	#0,D1
	moveq	#0,D0
	move.w	track,D1
	divu	#10,D1		; D0 => Y = 10ner * 8
	move.w	D1,D0
	lsl.w	#3,D0		; Y*8
	add.w	#111,D0		; Y+111
	mulu	#BPR*DEP,D0
	add.l	D0,A1
	clr.w	D1	
	swap	D1	
	lsl.w	#3,D1		; D1 => X = 1ner * 8
	tst.w	head
	bne.s	1$
	add.w	#141,D1
	bra	miniout
1$	add.w	#235,D1

miniout:
	st	blitter		; Blitter sperren = $FF
	moveq	#0,D2
	move.w	D1,D2
	lsr.w	#3,D1
	add.w	D1,A1
	bsr	waitblit
	and.w	#$000F,D2
	swap	D2
	lsr.l	#4,D2
	move.w	D2,con1(A6)
	ori.w	#$0FF2,D2
	move.w	D2,con0(A6)	; D=( C AND !B) OR A
	clr.w	amod(A6)
	clr.w	bmod(A6)
 	move.w	#BPR*(DEP-1)+36,cmod(A6)
	move.w	#BPR*(DEP-1)+36,dmod(A6)

	lea	zahl,A0
1$	move.l	D3,D0
	mulu	#20,D0		; Error * Offset
	add.l	D0,A0
	moveq	#DEP-1,D0	; Planes-1= 5 Planes
mo1
	clr.l	afwm(A6)
	lsr.w	#1,D4
	bcc.s	1$
	move.l	#$ffffffff,afwm(A6)	; letztes WORD = 0
1$	move.l	A0,apth(A6)
	move.l	#mask,bpth(A6)
	move.l	A1,cpth(A6)
	move.l	A1,dpth(A6)
	move.w	#(5<<6)!2,size(A6)
	bsr	waitblit
	add.w	#BPR,A1
	dbf	D0,mo1
	sf	blitter	; Blitter freigeben = $00
nodisp	rts

;-------------------------------------------------------------
;
;	Diverses
;
CreateMask
	lea	Icons,A0
	move.l	A0,D7		; D7=Base
	move.l	#MSKSTO,mskpth	; Zeiger auf Maskenspeicher in A3
	move.l	mskpth,A3
crm0	move.l	(A0),D0		; BASE wird dazu addiert !!!
	cmp.l	#$44114411,D0
	beq	return
	move.l	D0,A1
	cmp.l	D7,D0		; 1.ter Aufruf
	bge.s	1$		; nein
	add.l	D7,A1		; ja, BASE addieren
1$	move.l	A1,(A0)+	; und speichern
	move.w	(A1)+,D0	; Breite in Worten
	lsl.w	#1,D0		; nun in Bytes
	move.w	(A1)+,D1	; Zeilen=Hoehe
	subq.w	#1,D1		; -1
	move.l	A3,(A1)+	; Pointer auf Maske speichern
	;			; A1 nun Zeiger auf beginn der Daten
crm2	move.l	A3,-(A7)
	move.w	D0,D5	
crm21	moveq	#DEP-1,D3	; DEP Bitplanes oren
	moveq	#0,D4
	move.l	A1,A2
crm3	or.w	(A2),D4
	add.w	D0,A2	; Modulo in Bytes addieren
	dbf	D3,crm3
	move.w	D4,(A3)+	; Maskenword speichern
	addq.l	#2,A1	; naechstes WORD
	subq.w	#2,D5
	bne.s	crm21

	moveq	#DEP-2,D3	; Zeile (DEP-1) mal reproduzieren
crm4	move.w	D0,D2
	lsr.w	#1,D2	; Anzahl der georten WORDS
	subq.w	#1,D2
	move.l	(A7),A2	; Anfang der ersten Zeile
1$	move.w	(A2)+,(A3)+
	dbf	D2,1$
	dbf	D3,crm4
	add.w	#4,A7
	move.w	D0,D2	; auf naechste Zeile positionieren
	mulu	#DEP-1,D2
	add.w	D2,A1
	dbf	D1,crm2	; Hoehe des Objectes
	move.l	A3,mskpth
	bra.s	crm0

 IFNE SOLO
KILLSYSTEM
decrunch
	rts
 ENDC

 IFEQ SOLO


;	*** Quit Requester
;	***----------------

w_quit:	WINDOW 32,85,32*8,35,DGREY,LGREY,0,w_qbo,w_qtx
DF	SET SHADOW!BACKGND
w_qtx:	DC.W	1-1
 IFD GER
	TXT_GADGET 4,4,RED,DF,<"Event.gleiche Disks entfernen!">
 ELSE
	TXT_GADGET 22,4,RED,DF,<"Remove equal disks, if any!">
 ENDC
w_qbo:	DC.W	2-1
 IFD GER
	BOX_GADGET 10,20,9*8+4,10,LGREY,1,YELLOW,<" BEENDEN ">
	BOX_GADGET 170,20,9*8+4,10,LGREY,2,YELLOW,<" ZURUECK ">
 ELSE
	BOX_GADGET 10,20,8*8+4,10,LGREY,1,YELLOW,<"  EXIT  ">
	BOX_GADGET 178,20,8*8+4,10,LGREY,2,YELLOW,<" CANCEL ">
 ENDC


;	*** KillSys Requester
;	***-------------------

w_ksys:	WINDOW 56,85,26*8,35,DGREY,LGREY,0,w_kbo,w_ktx

DF	set SHADOW!BACKGND ; Zeichenflags
w_ktx	DC.W	1-1
 IFD GER
	TXT_GADGET 4,4,RED,DF,<"Systembereiche loeschen ?">
 ELSE
	TXT_GADGET 20,4,RED,DF,<"Clear system areas ?">
 ENDC
w_kbo	DC.W	2-1
 IFD GER
	BOX_GADGET 10,20,6*8+4,10,LGREY,1,YELLOW,<"  OK  ">
	BOX_GADGET 145,20,6*8+4,10,LGREY,2,YELLOW,<"NICHT">
 ELSE
	BOX_GADGET 10,20,6*8+4,10,LGREY,1,YELLOW,<"  OK  ">
	BOX_GADGET 145,20,6*8+4,10,LGREY,2,YELLOW,<"  NO  ">
 ENDC

;	*** Quit
;	***------
Quit:	tst.b	mouseflag	; wenn auch mousefreeze->exit now!
	bne	Bye
	;
	lea	.checkin(pc),a0
	moveq	#GREEN,d0
	bsr	Status
	;
	move.b	Drives,d2
	moveq	#3,d3		; zähler für drivenummern
	moveq	#0,d4		; zähler für eingelegte disks
	;
.chkins	btst	d3,d2		; drive angeschlossen ?
	beq.s	.next		; nein, dann skippen
	moveq	#1,d0
	lsl.b	d3,d0
	movem.l	d2-d4,-(sp)
	bsr.s	TestDrive
	movem.l	(sp)+,d2-d4
	tst.l	d1
	bne	.next
	addq.l	#1,d4		; sonst zähler erhöhen
.next	dbf	d3,.chkins
	;
	cmp.w	#2,d4		; mindestens zwei disks eingelegt ?
	blt.s	.quit
	;
	lea	w_quit,A0	; quit-requester...
	bsr	x_OpenWindow	; ...aufmachen
	move.l	D0,-(SP)	; windowpointer speichern
	lea	w_quit,A0	; quit-gadgets...
	bsr	x_CheckGadgets	; ...abfragen
	move.w	D0,D6		; rückgabewert speichern
	bsr	ReleaseLMB	; nochmal maus testen
	RestoreWindow (SP)+
	cmp.w	#1,d6		; exit button gedrückt ?
	bne.s	.back		; nein, dann zurück ins xcopy
	;
.quit	bsr	DrivesOFF
	bra	Bye		; exit hard!
	;
.back	bsr	StatusClear
	bra	DrivesOFF
	;
 IFD GER
.checkin	DC.B "Teste Laufwerke...",0
 ELSE
.checkin	DC.B "Checking drives...",0
 ENDC
		EVEN

;	*** TestDrive (DiskInserted, inserted=-1, not inserted=0)
;	***-------------------------------------------------------
TestDrive:
	move.l	D0,-(SP)
	asl.b	#3,D0
	eor.b	#$FF,D0
	move.b	#$7F,$bfd100
	nop
	nop
	moveq	#$7F,D1
	and.b	D0,D1
	move.b	D1,$bfd100
	bsr	bigdelay
	bsr	bigdelay
	StartTimer 617000	; us (917000)
.tstrdy	btst	#5,$BFE001	; 20 Takte
	beq.s	.dskrdy		; 10 Takte
	TestTimer .tstrdy
	moveq	#1,D1
	bra.s	.dskerr
	;
.dskrdy	moveq	#0,D1
.dskerr	move.l	(SP)+,D0
	tst.l	D1		; FLAGS setzen!!
	rts
	


; ** Copyright-Window anzeigen

TCOL	set RED
w_copyright
	WINDOW 80,60,160,80,31,9,0,0,.wpyr
DF	set SHADOW!BACKGND
.wpyr	DC.W	5-1
	TXT_GADGET 16,5,TCOL,DF,<"CACHET SOFTWARE">
	TXT_GADGET 24,20,TCOL,DF,<"OSTENDSTR. 32">
	TXT_GADGET 16,35,TCOL,DF,<"7524 OESTRINGEN">
	TXT_GADGET 28,50,TCOL,DF,<"WEST GERMANY">
	TXT_GADGET  4,65,TCOL,DF,<"Tel.: 07253 - 22411">	
	
	EVEN
Copyright:
	lea	w_copyright,A0
	bsr	x_OpenWindow	; in D0 Zeiger auf Puffer
	bsr	mouseclick 
	RestoreWindow D0
	rts
 ENDC ; (IFEQ SOLO)	

mouseclick
	btst	#6,$BFE001	; auf erste Maustaste warten
	bne.s	mouseclick
ReleaseLMB
	btst	#6,$BFE001	; nicht Prellen
	beq.s	ReleaseLMB
	rts


;--------------------------------------------------------------------------
;
;	KeyBoard und Eingaberoutine
;
;

LSHIFT	= 0
RSHIFT	= 1
CAPS	= 2
CONTROL	= 3
LALT	= 4
RALT	= 5
LAMIGA	= 5
RAMIGA	= 6
BS	= 8
TAB	= 9
CR	= 13

F1	= 14
F2	= 15
F3	= 16
F4	= 17
F5	= 18
F6	= 19
F7	= 20
F8	= 21
F9	= 22
F10	= 23

DEL	= 26
ESC	= 27	
CRSUP	= 28
CRSDO	= 29
CRSRG	= 30
CRSLF	= 31


KEYUP	equr D1
KeyStatus	DC.B	$00
KeyBuf	DC.B	$FF
KeyDelay	DC.W	$00
CurFLG	DC.W	$00

GetKey:	moveq	#0,D0
	tst.w	KeyDelay
	bne	return	; KeyBuf nicht loeschen!
	cmp.b	#$FF,KeyBuf
	beq	return
	
GetKeyNow:
	movem.l	D1/A0,-(SP)
	clr.w	KEYUP
	moveq	#0,d0
	move.b	KeyBuf,D0
	btst	#7,D0	; KEY-UP ?
	beq.s	toasci	; Nein
	bclr	#7,D0
	moveq	#1,KEYUP
toasci	lea	ktab(pc),A0
	cmp.w	#$40,D0
	bgt.s	sonder
	btst	#LSHIFT,KeyStatus
	bne.s	1$
	btst	#RSHIFT,KeyStatus
	bne.s	1$
	btst	#CAPS,KeyStatus
	beq.s	key_ok
1$	lea	kshift(pc),A0

key_ok	tst.w	KEYUP
	bne.s	ill_key
	move.w	#10,KeyDelay	; ca. 1/3 Sekunde als Delay
	move.b	(A0,D0.W),D0
	bra.s	gkey_ret

sonder	cmp.w	#$59,D0
	ble.s	key_ok	; ktab verwenden und Sondercode holen		
	cmp.w	#$60,D0	; QualifierKey ?
	blt.s	ill_key
	cmp.w	#$67,D0
	bgt.s	ill_key
	sub.w	#$60,D0
	tst.w	KEYUP
	bne.s	1$
	bset	D0,KeyStatus
	bra.s	ill_key
1$	bclr	D0,KeyStatus
ill_key	moveq	#0,D0
	move.b	#$FF,KeyBuf
gkey_ret
	movem.l	(SP)+,D1/A0
	rts

 IFD GER
	; *** Deutsche Keymap ***
ktab	DC.B	"`1234567890",0,"'",$5C,$00,"0qwertzuiop",0,"+",$00   ; $00-$1C
	DC.B	"123asdfghjkl",0,0,$00,$00,"456",$00         ; $1D-$30
	DC.B	"yxcvbnm,.-",$00,".789 "                   ; $31-$40
	DC.B	BS,TAB,CR,CR,ESC,DEL,$00,$00,$00,"-",$00   ; $41-$4B
	DC.B	CRSUP,CRSDO,CRSRG,CRSLF,F1,F2,F3,F4,F5,F6  ; $4C-$55
	DC.B	F7,F8,F9,F10                               ; $56-$59

kshift	DC.B	'~!"§$%&/()=?`|',$00,"0QWERTZUIOP",0,"*",$00   ; $00-$1C
	DC.B	"123ASDFGHJKL",0,0,$00,$00,"456",$00      ; $1D-$30
	DC.B	"YXCVBNM;:_",$00,".789 "                   ; $31-$40
 ELSE

	; *** US Keymap ***
ktab	DC.B	"`1234567890-=",$5C,$00,"0qwertyuiop[]",$00   ; $00-$1C
	DC.B	"123asdfghjkl;'",$00,$00,"456",$00         ; $1D-$30
	DC.B	"zxcvbnm,./",$00,".789 "                   ; $31-$40
	DC.B	BS,TAB,CR,CR,ESC,DEL,$00,$00,$00,"-",$00   ; $41-$4B
	DC.B	CRSUP,CRSDO,CRSRG,CRSLF,F1,F2,F3,F4,F5,F6  ; $4C-$55
	DC.B	F7,F8,F9,F10                               ; $56-$59

kshift	DC.B	"~!@#$%^&*()_+|",$00,"0QWERTYUIOP{}",$00   ; $00-$1C
	DC.B	"123ASDFGHJKL:",$22,$00,$00,"456",$00      ; $1D-$30
	DC.B	"ZXCVBNM<>?",$00,".789 "                   ; $31-$40
 ENDC

	EVEN

Input:	clr.w	copy_active
	clr.w	CurFLG
	clr.w	vbl
	lea	buf,A1	; Eingabepuffer
	move.l	#'NAME',(A1)+
	move.w	#$3A20,(A1)+
	moveq	#0,D7	; Laenge
	;
iout	move.b	#'_',(A1)	; Cursor
	tst.w	CurFLG
	beq.s	1$
	move.b	#' ',(A1)
1$	clr.b	1(A1)
	movem.l	D7/A1,-(SP)
	lea	buf,A0
	moveq	#GREEN,D0
	bsr	StatusInput		;added (H.V 18-9-91)
	movem.l	(SP)+,D7/A1
busy	cmp.w	#10,vbl	
	blt.s	1$	

;	clr.w	vbl

	bsr	x_VBLWait		;added (H.V 18-9-91)

	eori.w	#1,CurFLG	
	bra.s	iout
	;
1$	bsr	GetKey
	tst.w	D0
	beq.s	busy
	cmp.w	#CR,D0
	bne.s	2$
	tst.w	D7
	bne.s	5$
	;
	; *** LEERSTRING: "Empty" als Namen (H.V 17-9-91)
	;
	lea	buf,a1		;wenn nur return, "Empty" als vorgabenamen
	lea	9$(pc),a0
	moveq	#-1,d7
6$	addq.l	#1,d7
	move.b	(a0)+,(a1)+
	bne.s	6$
	subq.l	#2,a1
	bra.s	7$
	;
5$	clr.b	(A1)
	lea	buf,A1
	moveq	#28,D0
4$	move.b	6(A1),(A1)+	; NAME: loeschen
	dbeq	D0,4$
7$	lea	buf,A0
	moveq	#GREEN,D0
	bsr	Status
	clr.l	SYSTime
	move.w	#1,copy_active
	rts
	;
2$	cmp.w	#BS,D0
	bne.s	3$
	tst.w	D7
	beq.s	busy
	clr.b	-(A1)
	subq.l	#1,D7
	bra	iout
	;
3$	cmp.w	#32,D0
	blt	busy
	cmp.w	#26,D7	; maximal 26 Zeichen einlesen
	bge	busy
	move.b	D0,(A1)+
	addq.l	#1,D7
	bra	iout
	;
 IFD GER
9$	DC.B "Leer",0
 ELSE
9$	DC.B "Empty",0
 ENDC
	EVEN

;
;  XCOPY MENUE MODULE
;	
;  Date      Who        Changes                                           
;  --------- ---------- --------------------------------------------------
;  07-SEP-90 Frank      XCOPY 3.0

IY	= 62

	BSSSEG
buf	DS.B	52

	DATASEG
;----------------------------------------------------------
;
; die Defaultwerte werden vom Unterprogramm SetDefault in
; die Variablen; starttrack bis Verify kopiert
;
 IFEQ SOLO
;SetupID	DC.L	'DSET'	; Kennung fuer SetupPRG: $44534554

	CNOP	0,4
	DC.L	$DEADC0DE
	DC.L	enddefault-default
 ENDC
default:
	DC.W	0,79,0,1,0	; starttrack,endtrack,starthead,endhead,side
	DC.W	DOSCOPY,0,$4489	; mode,device,sync
	DC.B	1,2,2		; Source,Target,Verify
enddefault:
Drives	DC.B	0
maxchip	DC.L	0		; bis hierhin werden die Werte an SOLO ubergeben
lofast	DC.L	0		; von default bis FREQ sind es 24 Bytes
hifast	DC.L	0		; = 6 LONGS
	DS.L	10

erdx	DC.W	0

sidetx	DC.L	s1,s2,s3

 IFD GER
s1	DC.B	"BEIDE",0
s2	DC.B	"OBEN ",0
s3	DC.B	"UNTEN",0
 ELSE
s1	DC.B	"BOTH ",0
s2	DC.B	"UPPER",0
s3	DC.B	"LOWER",0
 ENDC
	EVEN
errtx	DC.L	e1,e2,e3,e4,e5,e6,e7,e8
  IFD GER
e1	DC.B	"1: Weniger oder mehr als 11 Sektoren!",0
e2	DC.B	"2: Kein Sync gefunden!",0
e3	DC.B	"3: Kein Sync nach GAP gefunden!",0
e4	DC.B	"4: Header Pruefsummen Fehler!",0
e5	DC.B	"5: Fehler im Header/Format Long!",0
e6	DC.B	"6: Datenblock Pruefsummen Fehler!!",0
e7	DC.B	"7: Langer Track!",0
e8	DC.B	"8: Verify Fehler!",0
  ELSE
e1	DC.B	"1: More or less that 11 sectors!",0
e2	DC.B	"2: No sync found!",0
e3	DC.B	"3: No sync after gap found!",0
e4	DC.B	"4: Header checksum error!",0
e5	DC.B	"5: Error in header/format long!",0
e6	DC.B	"6: Datablock checksum error!",0
e7	DC.B	"7: Long track!",0
e8	DC.B	"8: Verify error!",0
  ENDC

devi	DC.L	b1,b2
b1	DC.B	"DISK",0
b2	DC.B	"RAM ",0


NOFLASH	= $7fff
NOWAIT	= $7ffe

ICON	MACRO
	DC.W	\1,PY,DX,DY,NR
	DC.L	\2,\3	
	ENDM


ICONTAB:

; Paramter fuer QUIT setzen
PY	set 0
DX	set 3
DY	set 3
NR	set NOFLASH
 IFNE SOLO
	; ICON 0,0,Bye			;bei solo kein exit!!!
 ELSE
	ICON 0,0,Quit
 ENDC
; Paramter fuer obere Leiste setzen
PY	set 45
DX	set 8
DY	set 8
NR	set 5

	ICON 10,0,OpenCopyW	;FlipCopy
	ICON 47,0,OpenToolW
	ICON 181,0,SideSelect
	ICON 227,0,SyncSelect
	ICON 271,0,ShowDevice

; Paramter fuer obere Buttonreihe setzen
PY	set 55
DX	set 8
DY	set 8
NR	set 3

	ICON 98,10,StartSelect
	ICON 106,1,StartSelect
	ICON 122,2,StartSelect	; 2 => toggle HEAD
	ICON 141,10,EndSelect
	ICON 149,1,EndSelect
	ICON 165,2,EndSelect	; 2 => toggle HEAD
	ICON 228,$1000,SyncM
	ICON 236,$100,SyncM
	ICON 244,$10,SyncM
	ICON 252,$1,SyncM

; Paramter fuer untere Buttonreihe setzen
PY	set 70
DX	set 8
DY	set 8
NR	set 4

	ICON 98,-10,StartSelect
	ICON 106,-1,StartSelect
	ICON 141,-10,EndSelect
	ICON 149,-1,EndSelect
	ICON 228,-$1000,SyncM
	ICON 236,-$100,SyncM
	ICON 244,-$10,SyncM
	ICON 252,-$1,SyncM

	
; Paramter fuer SourceSelect setzen
PY	set 106
DX	set 8
DY	set 16
NR	set NOFLASH

	ICON 16,0,SourceSelect
	ICON 47,1,SourceSelect
	ICON 78,2,SourceSelect
	ICON 109,3,SourceSelect

; Paramter fuer TargetSelect setzen
PY	set 155
DX	set 8
DY	set 16
NR	set NOFLASH

	ICON 16,0,TargetSelect
	ICON 47,1,TargetSelect
	ICON 78,2,TargetSelect
	ICON 109,3,TargetSelect

; Paramter fuer AboutGadgets setzen
PX	set 12
PY	set 9
DX	set 48
DY	set 16
NR	set NOWAIT
	ICON 12,0,ShowAbout
	ICON 260,0,ShowCoders

; Paramter fuer Execute als letzte setzen
PY	set 81
DX	set 39
DY	set 6
NR	set 6

	ICON 11,0,Executor	 ; Start
	ICON 140,NAME,SetRMode	 ; DiskInfo alt: Name
	ICON 183,DIR,SetRMode	 ; alt: Setdefault
	ICON 226,CHECK,SetRMode ; alt: ListError
	ICON 269,0,SetDefault
	ICON -1,-1,-1

;-------------------------

STOPICON	
	ICON 97,0,UserBreak
	ICON -1,-1,-1

;-------------------------

RAMICON	ICON 11,0,dummy	; Start
	ICON 54,1,dummy	; Continue
	ICON 97,2,dummy	; Stop
	ICON -1,-1,-1

;-------------------------
	
	CODESEG

	EVEN

TestStop
	btst	#6,$BFE001	; wird nur aus TPS_Server aufgerufen
	bne	return		; linke Maustaste nicht gedrueckt
	lea	STOPICON,A0	; Mausclick uber StopIcon?
	bra	checkicon

RamHandler	
	clr.w	copy_active	; wird nur von RAM-Copy gerufen
	move.l	SYSTime,-(SP)	; Systemzeit zwischenspeichern
1$	btst	#6,$BFE001
	bne.s	1$
	lea	RAMICON,A0
	bsr	checkicon
	tst.w	D2		; gueltiger Klick?
	bmi.s	1$		; NEIN
	move.l	(SP)+,SYSTime
	move.w	#1,copy_active
dummy	rts	

IconHandler:
 IFEQ SOLO
  IFD DOSETUP
	move.l	SetupID,D0
	cmp.l	$100,D0
	bne.s	nodefreq
	move.w	#$4e75,D0	; bestimmte Funktionen sperren, wenn XCOPY
	lea	Executor(pc),A0	; im Setup-Modus gestartet wurde
	move.w	D0,(A0)
	lea	SetRMode(pc),A0
	move.w	D0,(A0)
	lea	KILLSYSTEM,A0
	move.w	D0,(A0)
	lea	ListError(pc),A0
	move.w	D0,(A0)

	lea	setdef(pc),a0
	moveq	#GREEN,d0
	bsr	Status
	bra	nodefreq
   IFD GER
setdef	DC.B	"BITTE DEFAULT SETZEN UND EXIT",0
   ELSE
setdef	DC.B	"PLEASE SET DEFAULTS AND EXIT",0
   ENDC
	EVEN

nodefreq
  ENDC
 ENDC


main_loop:
	btst	#6,$BFE001	; Left Mouse Button
	bne.s	main_loop	; nicht gedrueckt
	lea	ICONTAB,A0
	bsr	checkicon
	bra.s	main_loop

checkicon
	bsr	MouseYPos	
	move.l	D0,D1		; D1=Y
	bsr	MouseXPos	; D0=X
iloop	move.w	(A0),D2		; XPOS
	bmi	return		; kein ICON gefunden, negative Rueckgabe
	move.w	2(A0),D3	; YPOS
	cmp.w	D2,D0
	blt.s	next
	cmp.w	D3,D1
	blt.s	next
	add.w	4(A0),D2	; XPOS+DX
	add.w	6(A0),D3	; YPOS+DY
	cmp.w	D2,D0
	bgt.s	next
	cmp.w	D3,D1
	bgt.s	next
	moveq	#0,D0
	moveq	#0,D1
	moveq	#0,D2
	move.w	(A0),D0	; X
	move.w	2(A0),D1	; Y
	move.w	8(A0),D2	; NR zum flashen
	cmp.w	#NOFLASH,D2
	bne.s	1$
	tst.w	d2
	VBLWait 8		; D0 scratch	
	bra.s	2$
	;
1$	cmp.w	#NOWAIT,d2
	beq.s	2$
	move.l	A0,-(A7)
	bsr	Flash		; Icon blinkenlassen
	move.l	(A7)+,A0
2$	move.l	10(A0),-(A7)	; Parameter auf Stack
	move.l	14(A0),A1	; Sprungadresse holen
	jsr	(A1)		; Routine ausfuehren
;	VBLWait 4
	Delay 75600
	moveq	#0,D2		; D2 indiziert Gueltigkeit PLUS/MINUS
	move.l	(A7)+,D0	; Parameter auch Rueckgabewert
	rts
next	lea	18(a0),a0
	bra	iloop
;----------------------------------------------
;
;
ListError
	move.w	erdx,D0
	lsl.w	#2,D0
	lea	errtx,A0
	move.l	(A0,D0.w),A0
	moveq	#BLUE,D0
	bsr	Status
	addq.w	#1,erdx
	cmp.w	#7,erdx
	ble	return
	clr.w	erdx
	rts

;	*** SideSelect
;	***------------
SideSelect:
	lea	side,a0
	addq.w	#1,(a0)
	cmp.w	#2,(a0)
	ble.s	1$
	clr.w	(a0)
1$	bra	ShowSide

;	*** StartSelect
;	***-------------
StartSelect:
	move.l	4(A7),D0
	cmp.w	#2,D0
	bne.s	1$
	eori.w	#1,starthead
	bra.s	TrackCon
1$	add.w	D0,starttrack
	bra.s	TrackCon

;	*** EndSelect
;	***-----------
EndSelect:
	move.l	4(A7),D0
	cmp.w	#2,D0
	bne.s	1$
	eori.w	#1,endhead
	bra.s	TrackCon
1$	add.w	D0,endtrack
;	bra	TrackCon

TrackCon:
	lea	starttrack,a0
	lea	endtrack,a1
	;
	tst.w	(a0)
	bpl.s	9$
	clr.w	(a0)
9$	tst.w	(a1)
	bpl.s	8$
	clr.w	(a1)
8$	cmp.w	#81,(a1)
	ble.s	1$
	move.w	#81,(a1)

1$	move.w	(a0),D0
	cmp.w	(a1),D0
	ble.s	2$
	move.w	(a1),(a0)
2$	move.w	starthead,D1
	cmp.w	endhead,D1
	blt.s	3$
	move.w	(a0),D0
	cmp.w	(a1),D0
	blt.s	3$
	move.w	starthead,endhead
3$	bsr	StartTrack
	bra	EndTrack
 
;	*** SourceSelect
;	***--------------
SourceSelect:
	lea	Source,a0
	move.l	4(A7),D0
	btst	D0,Drives
	beq	return3	; angeklicktes Drive nicht vorhanden
	btst	D0,(a0)
	beq.s	1$
	bclr	D0,(a0)	; Drive deselektieren
	bra.s	targetoff
1$	clr.b	(a0)
	bset	D0,(a0)		; Drive selektieren
targetoff	
	tst.w	device
	bne.s	3$	; RAM Mode on
	bclr	D0,Target	; Drive als Target abwaehlen
	bclr	D0,Verify
	bclr	D0,OldVerify
3$	bra	ShowSelect

;	*** TargetSelect
;	***--------------
TargetSelect:
	lea	Target,a0
	move.l	4(A7),D1
	btst	D1,Drives
	beq	return3	; angeklicktes Drive nicht vorhanden
	btst	D1,(a0)
	bne.s	1$	; Target ist schon angewaehlt 
	bset	D1,(a0)	; Target selektieren
	bra.s	9$
	;
1$	move.w	mode,d0	;
	bsr	tstverify	; es ist ein mode angewahlt der kein
	bne.s	2$		; kein Verify zulaesst	

   	btst	D1,Verify
	bne.s	2$	; Verify ist schon ON => OFF schalten
	bset	D1,Verify	; Verify ON setzen
	bra.s	9$
2$	bclr	D1,Target		; Drive als Target abwaehlen
	bclr	D1,Verify
	bclr	D1,OldVerify
9$	tst.w	device
	bne.s	3$
	bclr	D1,Source
3$	bra	ShowSelect
return3	rts

;	*** ShowSelect
;	***------------
ShowSelect:
	moveq	#16,D7		; X-Koor
	moveq	#0,D6		; D6 TestBit
drloop	btst	D6,Drives
	beq.s	nextdr
	movem.l	D6-D7,-(A7)
	moveq	#0,D2		; IconNR
	btst	D6,Source
	beq.s	1$
	moveq	#1,D2
1$	move.l	D7,D0		; X
	move.w	#106,D1		; Y
	bsr	DrawMap
	movem.l	(A7),D6-D7
	moveq	#0,D2
	btst	D6,Target	
	beq.s	3$
	moveq	#1,D2
	btst	D6,Verify
	beq.s	3$
	moveq	#2,D2
3$	move.l	D7,D0		; X
	move.w	#155,D1		; Y
	bsr	DrawMap
	movem.l	(A7)+,D6-D7
	;
nextdr	add.w	#31,D7		; X=X+31
	addq.w	#1,D6
	cmp.w	#4,D6
	blt.s	drloop
	rts

;	DATASEG
tform	DC.B	"%02ld %1ld",0
	EVEN


StartTrack
	moveq	#0,D0
	move.w	starthead,D0
	move.l	D0,-(A7)
	moveq	#0,D0
	move.w	starttrack,D0
	move.l	D0,-(A7)
	XPRINTF buf,tform,8	; 8  => 2 Longs vom Stack holen
	moveq	#98,D0
	moveq	#IY,D1
	moveq	#DGREEN,D2
	lea	buf,A0
	bra	x_Print

EndTrack
	moveq	#0,D0
	move.w	endhead,D0
	move.l	D0,-(A7)
	moveq	#0,D0
	move.w	endtrack,D0
	move.l	D0,-(A7)
	XPRINTF buf,tform,8	; 8  => 2 Longs vom Stack holen
	move.w	#141,D0
	moveq	#IY,D1
	moveq	#DGREEN,D2
	lea	buf,A0
	bra	x_Print

;	DATASEG
	
sform	DC.B	"%04lx",0
indxtx	DC.B	"INDX",0
	EVEN

 IFEQ SOLO
FCOL	set DGREY
BCOL	set LGREY
TCOL 	set YELLOW
 ELSE
FCOL	set BLACK	;DGREY
BCOL	set DGREY
TCOL	set DGREY	;LGREY
 ENDC

w_copy:	WINDOW 10,53,76,47,FCOL,BCOL,w_c,0,0
w_c	DC.W	4-1
	STR_GADGET 2,5,TCOL,DOSCOPY,<"DOSCOPY  ">
	STR_GADGET 2,15,TCOL,DOSPLUS,<"DOSCOPY+ ">
	STR_GADGET 2,25,TCOL,BAMCOPY,<"BAMCOPY+  ">
	STR_GADGET 2,35,TCOL,NIBBLE,<"NIBBLE   ">

 IFEQ SOLO
w_tools	WINDOW 40,53,76,90,FCOL,BCOL,w_t,0,0
 ELSE
w_tools	WINDOW 40,53,76,76,FCOL,BCOL,w_t,0,0
 ENDC
w_t
 IFEQ SOLO
	DC.W	8-1	; ein Menuepunkt mehr bei MULTI
 ELSE
	DC.W	7-1
 ENDC
	STR_GADGET 2,5,TCOL,OPTIMIZE,<"OPTIMIZE ">
	STR_GADGET 2,15,TCOL,FORMAT,<"FORMAT   ">
	STR_GADGET 2,25,TCOL,QFORMAT,<"QFORMAT  ">
 IFD GER
	STR_GADGET 2,35,TCOL,ERASE,<"LOESCHEN ">
 ELSE
	STR_GADGET 2,35,TCOL,ERASE,<"ERASE    ">
 ENDC
	STR_GADGET 2,45,TCOL,INSTALL,<"INSTALL  ">
 	STR_GADGET 2,57,TCOL,MESLEN,<"SPEEDCHK ">
 	STR_GADGET 2,67,TCOL,DRIVESON,<"DRIVES ON">
 IFEQ SOLO
	STR_GADGET 2,80,RED,KILLSYS,<"KILLSYS  ">
 ENDC


	CODESEG

tstverify
	cmp.b	#DOSCOPY,D0	; Z-Flag (beq) wird gesetzt, wenn D0
	beq.s	.v_on		; einen der aufgefuehrten Modi enthaelt
	cmp.b	#DOSPLUS,D0
	beq.s	.v_on
	cmp.b	#BAMCOPY,D0
	beq.s	.v_on
	cmp.b	#FORMAT,D0
	beq.s	.v_on
	cmp.b	#QFORMAT,D0
	beq.s	.v_on
	cmp.b	#INSTALL,d0
.v_on	rts

OpenCopyW
	lea	w_copy(pc),A0
	bra.s	s_menue
OpenToolW
	lea	w_tools(pc),A0

s_menue	move.l	A0,A1			; WindowPointer merken
	bsr	x_OpenWindow
	move.l	D0,-(SP)
	move.l	A1,A0			; Gadgets in Window abfragen
	bsr	x_CheckGadgets
	move.l	D0,D1			; Rueckgabe sichern
	bsr	ReleaseLMB
	RestoreWindow (SP)+

 IFEQ SOLO
	cmp.w	#KILLSYS,D1
	beq	KILLSYSTEM
 ENDC
	cmp.w	#DRIVESON,d1
	bne.s	nodrivson

;	*** DRIVES ON ***
;	***-----------***
	movem.l	d0-d7/a0-a6,-(sp)
	cmp.w	#DRIVESON,mode
	beq.s	.nohold
	move.w	mode,oldmode
.nohold
	move.b	Drives,StartupDrives
	move.b	#%1111,Drives
	lea	.switchontxt(pc),a0
	moveq	#GREEN,d0
	bsr	Status
	bsr	ShowSelect
	move.w	oldmode,mode
	bsr	ShowMode
	movem.l	(sp)+,d0-d7/a0-a6
	rts
	;
 IFD GER
.switchontxt	DC.B "Alle Laufwerke einschalten!",0
 ELSE
.switchontxt	DC.B "Switch all drives on!",0
 ENDC
	EVEN
	BSSSEG
StartupDrives	DS.B 2
oldmode		DS.W 1
	CODESEG

nodrivson
	move.w	mode,D0	
	
	bsr	tstverify		; erlaubt alter Modus Verfiy ?
	bne.s	1$
	move.b	Verify,OldVerify	; ja, Verify Status sichern
1$	move.l	D1,D0
	bsr	tstverify		; erlaubt neuer Modus verify
	beq.s	set_ver 
	clr.b	Verify			; neuer Modus erlaubt kein Verify
	bra.s	set_mod
set_ver	move.b	OldVerify,Verify	; alten Verify Status restaurieren
set_mod	move.w	D0,mode			; neuen Mode setzen
	bsr	ShowSelect		; falls sich bei Verify was geaendert hat

ShowMode
	move.w	mode,d0
	lea	w_copy(pc),A1
1$	move.l	WIN_STR(A1),A1
	move.w	(A1)+,D2		; Anzahl der Eintraege
2$	cmp.b	GAD_RET(A1),D0
	beq.s	3$
	adda.w	GAD_LEN(A1),A1
	dbf	D2,2$
	lea	w_tools,A1		; Gefahr einer Endlos-Schleife!!
	bra.s	1$			; wenn mode nicht gefunden wird
	;
3$	lea	GAD_STR(A1),A0		; Zeiger auf String holen
	moveq	#9,D0			; X
	moveq	#IY,D1			; Y
	moveq	#DGREEN,D2		; Color
	bra	x_Print
	
ShowSide
	move.w	side,D0
	lsl.w	#2,D0
	lea	sidetx,A0
	move.l	(A0,D0.w),A0
	move.w	#181,D0
	moveq	#IY,D1
	moveq	#DGREEN,D2
	bra	x_Print


Bit2Num	cmp.b	#1,D0
	bne.s	1$
	moveq	#0,D0
	rts
	;
1$	cmp.b	#2,D0
	bne.s	2$
	moveq	#1,D0
	rts
	;
2$	cmp.b	#4,D0
	bne.s	3$
	moveq	#2,D0
	rts
	;
3$	moveq	#3,D0
	rts


;	*** STATUS
;	*** ------
;	D0=COL, A0=StringPTH
	
StatusInput:
	movem.l	a0/d2,-(a7)		;gibt status plus 1 leerzeichen aus
	move.l	d0,d2			;für input-routine (kein flickern mehr!)
	moveq	#8,D0			;(H.V 19-9-91)
	move.w	#188,D1
	bsr	x_Print
	movem.l	(a7)+,a0/d2
	move.l	a0,a1
	moveq	#0,d0
1$	addq.l	#8,d0
	tst.b	(a1)+
	bne.s	1$
	move.w	#188,d1
	lea	90$(pc),a0
	bra	x_Print
90$	DC.B	" ",0
	EVEN
	;
Status:	movem.l	D0/A0,-(A7)
	bsr.s	StatusClear
	movem.l	(A7)+,D2/A0
	moveq	#8,D0
	move.w	#188,D1
	bra	x_Print
	;
StatusClear:
	moveq	#8,D0
	move.w	#188,D1
	moveq	#32,D2
	lea	BLANK(pc),A0
	bra	x_Print

BLANK	DC.B	"                                      ",0
 IFD GER
nosource	DC.B	"Keine Quelle gewaehlt!",0
notarget	DC.B	"Kein Ziel gewaehlt!",0
noopti		DC.B	"Bitte nur ein Ziel waehlen!",0
 ELSE
nosource	DC.B	"No source selected!",0
notarget	DC.B	"No destination selected!",0
noopti		DC.B	"Please select ONE destination!",0
 ENDC
	EVEN



SetRMode:
	move.l	4(A7),D0	; neuen mode vom Stack holen
	move.w	mode,-(SP)
	move.w	D0,mode
	bsr	Executor
	move.w	(SP)+,mode
	rts
	
QuitProg:
	move.w	#QUIT,mode
	
Executor:
	move.w	mode,D1   
	cmp.w	#QUIT,d1
	beq	exe
	cmp.w	#BAMCOPY,D1
	bne.s	9$
	pea	NAME		; rekursiver Aufruf von Executor ueber
	bsr	SetRMode	; SetRMode, um fuer das BAMCOPY eine 
	addq.l	#4,SP		; gueltige BAM-Map zu erhalten
	tst.w	D0
	bne	exok		; in DiskInfo ist ein Fehler aufgetreten	

9$	tst.b	Source
	bne.s	2$	
	cmp.w	#ERASE,d1	; Source braucht nicht angewaehlt sein
	beq.s	2$
	cmp.w	#FORMAT,d1
	beq.s	2$
	cmp.w	#QFORMAT,D1
	beq.s	2$
	cmp.w	#OPTIMIZE,d1
	beq.s	2$
	cmp.w	#MESLEN,D1
	beq.s	2$
	cmp.w	#INSTALL,d1
	beq.s	2$
	lea	nosource,A0	; Source muss bei allen COPY-Modi und
1$	moveq	#RED,D0		; bei den READONLY-Modi (CHECK,DIR,NAME)
	bsr	Status		; angewaehlt sein
	bra	ill

2$	tst.b	Target		; Target braucht nicht angewaehlt sein
	bne.s	3$
	cmp.w	#CHECK,d1
	beq.s	3$
	cmp.w	#NAME,d1
	beq.s	3$
	cmp.w	#DIR,d1
	beq.s	3$
	lea	notarget,A0
	bra.s	1$

3$	cmp.w	#OPTIMIZE,d1	; es darf nur 1 Target drive angewaehlt sein
	beq.s	8$
	cmp.w	#MESLEN,D1
	bne.s	exe
	;
8$	moveq	#3,D0		; target's zaehlen
	moveq	#0,D1
4$	btst	D0,Target
	beq.s	5$
	addq	#1,D1
5$	dbf	D0,4$
	cmp.w	#1,D1
	beq.s	exe
	lea	noopti(pc),A0	; Fehlermeldung ausgeben
	bra.s	1$ 

exe	bsr	StatusClear
	bsr	Start
	move.l	D0,D1		; in D1 Drive oder Fehlernr.	
	lsr.l	#8,D1
	and.l	#$FF,D0		; in D0 return code

	tst.w	D0
	bne.s	ex1
	cmp.w	#NAME,mode	; OK return
	beq	exok
	lea	4$(pc),A0	; Operation complete 
	moveq	#GREEN,D0
	bsr	Status
	TON_OK
	bra	exok
 IFD GER
4$	DC.B	"Operation ausgefuehrt.",0
 ELSE
4$	DC.B	"Operation done.",0
 ENDC
	EVEN

ex1
	cmp.w	#1,D0
	bne.s	ex2
	bra	ill	; SpecialERR
;	lea	4$,A0
;iout	moveq	#RED,D0
;	bsr	Status
;	bra	ill
;4$	DC.B	"Fehler auf den roten Positionen !",0
;	EVEN

ex2	cmp.w	#2,D0
	bne.s	ex3
	lea	4$(pc),A0
	moveq	#GREEN,D0
	bsr	Status
	bra	exok
 IFD GER
4$	DC.B	"Anwender Stop!",0
 ELSE
4$	DC.B	"User break!",0
 ENDC
	EVEN

ex3	cmp.w	#3,D0
	bne.s	ex4
NoIndexIN	
	move.l	D1,-(A7)	; Drive auf Stack
	pea	nin		; Message
parout	pea	buf
	bsr	Format
	add.w	#12,A7
	lea	buf,A0
	moveq	#RED,D0
	bsr	Status
	bra	ill

 IFD GER
nin	DC.B	"Kein Index Signal auf DF%1ld: !",0
 ELSE
nin	DC.B	"No index signal on DF%1ld: !",0
 ENDC
	EVEN

ex4	cmp.w	#4,D0
	bne.s	ex5
	move.l	D1,-(A7)	; Drive auf Stack
	pea	4$		; Message
	bra	parout
 IFD GER
4$	DC.B	"Verify Fehler auf Disk in DF%1ld: !",0
 ELSE
4$	DC.B	"Verify error on disk in DF%1ld: !",0
 ENDC
	EVEN

ex5	cmp.w	#5,D0
	bne.s	ex6
WritePROT
	move.l	D1,-(A7)	; Drive auf Stack
	pea	4$		; Message
	bra	parout
 IFD GER
4$	DC.B	"DF%1ld: ist schreibgeschuetzt !",0
 ELSE
4$	DC.B	"DF%1ld: is write protected!",0
 ENDC
	EVEN

ex6	cmp.w	#6,D0
	bne.s	ex7
NoDiskIN
	move.l	D1,-(A7)	; Drive auf Stack
	pea	4$	; Message
	bra	parout
 IFD GER
4$	DC.B	"Keine Disk in DF%1ld: !",0
 ELSE
4$	DC.B	"No disk in DF%1ld: !",0
 ENDC 
	EVEN

ex7	cmp.w	#7,D0
	bne.s	ex8
	move.l	D1,-(A7)	; Errorcode auf Stack
	pea	4$		; Message
	bra	parout
 IFD GER
4$	DC.B	"Optimize Fehler #%1ld !",0
 ELSE
4$	DC.B	"Optimize error #%1ld !",0
 ENDC
	EVEN

ex8	cmp.w	#8,D0
	bne.s	def
	lea	4$(pc),A0	; Message
	moveq	#RED,D0
	bsr	Status
	bra	ill
 IFD GER
4$	DC.B	"Kein Speicher fuer Diskpuffer!",0
 ELSE
4$	DC.B	"No memory for diskbuffer!",0
 ENDC
	EVEN


def	lea	nomes(pc),A0
	moveq	#2,D0
	bsr	Status
ill	TON_BAD
	moveq	#1,D0
	rts
exok	moveq	#0,D0
	rts
 IFD GER
nomes	DC.B	"Keine regulaere Message!!!",0
 ELSE
nomes	DC.B	"No regular message!",0
 ENDC
	EVEN

;---------------------------------------------------------------------
; 
;	SPRINTF     
;	- einfache Ersatzloesung, erkennt %d,%x,%c,%s und %ld,%lx
;	PARMS: 4(A7) -> Pointer auf Ausgabepuffer
;	       8(A7) -> Formatstring    
;                12(A7) -> alle Werte fuer Formatstrimg
;	REGS : D0-D6/A0-A3
;
;

Format:	movem.l	D0-D6/A0-A3,-(SP)
	move.l	4+44(A7),A2	; Pointer auf Ausgabepuffer
	move.l	8+44(A7),A3	; Pointer auf Formatstring
	lea	12+44(A7),A0	; Start der Ausgabewerte auf dem Stack
	move.l	A0,-(A7)	; Pointer auf Werte 
fc20f4	move.b	(A3)+,D0
	beq.s	for_end
	cmpi.b	#"%",D0		; % erkannt
	beq.s	fc210c
fc20fe	move.b	D0,(A2)+
	bra.s	fc20f4
for_end	addq.l	#4,A7		; A0 vom Stack holen
	move.b	D0,(A2)+	; NULL als Abschluss in Puffer
	movem.l	(SP)+,D0-D6/A0-A3
	rts

fc210c	lea	tmpbuf,A1
	clr.w	D3
	cmpi.b	#"-",(A3)	; Erstes Zeichen '-' ?
	bne.s	1$
	bset	#0,D3		; ja, Ausgabe linksbuendig
	addq.l	#1,A3
1$	cmpi.b	#"0",(A3)	; naechstes Zeichen '0' ?
	bne.s	2$	
	bset	#1,D3		; ja, fuehrende Nullen ausgeben
2$	bsr	Dec2Bin
	move.w	D0,D6
	clr.l	D5
	cmpi.b	#",",(A3)	; '.'
	bne.s	fc213e
	addq.w	#1,A3
	bsr	Dec2Bin
	move.w	D0,D5
fc213e	cmpi.b	#"l",(A3)	; 'l'
	bne.s	fc214a
	bset	#2,D3		; WORD-Ausgabe
	addq.w	#1,A3
fc214a	move.b	(A3)+,D0
	cmpi.b	#"d",D0		; 'd'
	bne.s	fc215a	
	bsr	getarg
	bsr	Bin2Dec
	bra.s	fc21a2
fc215a	cmpi.b	#"x",D0		; 'x'
	bne.s	fc2188
	bsr	getarg		; holt LONG oder WORD vom Stack
	bsr	Bin2Hex
	bra.s	fc21a2

getarg	btst	#2,D3
	bne.s	1$
	move.l	4(A7),A0	; WORD vom Stack holen
	move.w	(A0)+,D4
	move.l	A0,4(A7)
	ext.l	D4
	rts
1$	move.l	4(A7),A0	; LONG vom Stack holen
	move.l	(A0)+,D4
	move.l	A0,4(A7)
	rts

fc2188	cmpi.b	#"s",D0		; 's' Zeichenkette ?
	bne.s	fc2196
	move.l	(A7),A0		; Pointer auf String vom Stack holen
	move.l	(A0)+,A1
	move.l	A0,(A7)
	bra.s	fc21a8
fc2196	cmpi.b	#"c",D0		; 'c' einzelnes Zeichen
	bne	fc20fe
	bsr.s	getarg
	move.b	D4,(A1)+
fc21a2	clr.b	(A1)
	lea	tmpbuf,A1
fc21a8	move.l	A1,A0
	bsr.s	GetLen
	tst.w	D5
	beq.s	fc21b6
	cmp.w	D5,D2
	bhi.s	fc21b8
fc21b6	move.w	D2,D5
fc21b8	sub.w	D5,D6
	bpl.s	fc21be
	clr.w	D6
fc21be	btst	#0,D3
	bne.s	fc21cc
	bsr.s	BlankOUT
	bra.s	fc21cc
fc21c8	move.b	(A1)+,(A2)+
fc21cc	dbf	D5,fc21c8
	btst	#0,D3
	beq	fc20f4
	bsr.s	BlankOUT
	bra	fc20f4
;-----------------------------------------------------------------
;	Nullen oder Leerzeichen ausgeben
;
BlankOUT	
	move.b	#" ",D2	; Blank
	btst	#1,D3
	beq.s	2$
	move.b	#"0",D2	; Null
	bra.s	2$
1$	move.b	D2,(A2)+
2$	dbf	D6,1$
	rts

GetLen	moveq	#-1,D2
1$	tst.b	(A0)+
	dbeq	D2,1$
	neg.l	D2
	subq.w	#1,D2
	rts

Dec2Bin:
	clr.l	D0	; konvertiert String zu Zahl in D0
	clr.l	D2
1$	move.b	(A3)+,D2
	cmpi.b	#$30,D2
	bcs.s	2$
	cmpi.b	#$39,D2
	bhi.s	2$
	add.l	D0,D0
	move.l	D0,D1
	add.l	D0,D0
	add.l	D0,D0
	add.l	D1,D0
	subi.b	#$30,D2
	add.l	D2,D0
	bra.s	1$
2$	subq.l	#1,A3
	rts


;----------------

Bin2Dec:
	tst.l	D4
	beq.s	con_ret
	bmi.s	1$
	neg.l	D4
	bra.s	2$
1$	move.b	#"-",(A1)+
2$	lea	Potenz(pc),A0
	clr.w	D1
3$	move.l	(A0)+,D2
	beq.s	con_ret
	moveq	#-1,D0
4$	add.l	D2,D4
	dbgt	D0,4$
	sub.l	D2,D4
	addq.w	#1,D0
	bne.s	5$
	tst.w	D1
	beq	3$
5$	moveq	#-1,D1
	neg.b	D0
	addi.b	#"0",D0
	move.b	D0,(A1)+
	bra	3$
con_ret	neg.b   D4
	addi.b  #"0",D4
	move.b  D4,(A1)+
	rts
	;
Potenz	DC.L 1000000000
	DC.L  100000000
	DC.L   10000000
	DC.L    1000000
	DC.L     100000
	DC.L      10000
 	DC.L       1000
	DC.L        100
	DC.L         10
	DC.L          0
	;
	BSSSEG
tmpbuf	DS.B	16
	CODESEG

Bin2Hex	tst.l	D4
	beq.s	.exit
	clr.w	D1
	btst	#2,D3
	bne.s	1$
	moveq	#$03,D2
	swap	D4
	bra.s	2$
1$	moveq	#$07,D2
2$	rol.l	#4,D4
	move.b	D4,D0
	andi.b	#$0F,D0
	bne.s	3$
	tst.w	D1
	beq.s	6$
3$	moveq	#-1,D1
	cmpi.b	#$09,D0
	bhi.s	4$
	addi.b	#$30,D0
	bra.s	5$
4$	addi.b	#$37,D0
5$	move.b	D0,(A1)+
6$	dbf	D2,2$
.exit	rts
;--------------------------------------------------------------------
;
;
SyncSelect
	cmp.w	#INDEXCOPY,sync
	bne.s	1$
	move.w	#$4489,sync
	bra.s	2$
1$	move.w	#INDEXCOPY,sync
2$	bra.s	ShowSync

SyncM	move.l	4(A7),D0
	add.w	D0,sync
;	bra.s	ShowSync

ShowSync
	cmp.w	#INDEXCOPY,sync
	bne.s	1$
	lea	indxtx,A0
2$	move.w	#228,D0
	moveq	#IY,D1
	moveq	#DGREEN,D2
	bra	x_Print
1$	moveq	#0,D0
	move.w	sync,D0
	move.l	D0,-(A7)
	XPRINTF buf,sform,4	
	lea	buf,A0
	bra.s	2$

ShowDevice:
	cmp.b	#1,Drives
	bne.s	1$
	move.w	#1,device
	bra.s	2$
1$	eori.w	#1,device
2$	move.w	device,D0
	lsl.w	#2,D0
	lea	devi,A0
	move.l	(A0,D0.W),A0	; StringPTH holen
	move.w	#271,D0
	moveq	#IY,D1
	moveq	#DGREEN,D2
	bsr	x_Print
	moveq	#0,D0
	move.b	Source,D0
	bsr	Bit2Num
	bra	targetoff	; bei RAM COPY Target abwaehlen

SetDefault:
	lea	default,A0
	lea	starttrack,A1
	moveq	#19-1,D0	; 19 Bytes kopieren
1$	move.b	(A0)+,(A1)+
	dbf	D0,1$

	move.b	Drives,D0
	and.b	D0,Source
	and.b	D0,Target
	and.b	D0,Verify
	move.b	Verify,OldVerify

	clr.w	erdx
	eori.w	#1,device

	bsr	ShowDevice

	bsr	ShowSync
	bsr	ShowMode
	bsr	StartTrack
	bsr	EndTrack
	bsr	ShowSelect
	bsr	ShowSide
 IFEQ  SOLO
  IFD DOSETUP
	move.l	SetupID,D1
	cmp.l	$100,D1
	beq.s	return4
  ENDC
 ENDC
	bsr.s	ShowFreeMem

return4	rts

ShowFreeMem:
	moveq	#0,D1	; Type: ALL
	jsr	AvailMem
	move.l	D0,-(A7)	; Memory in Bytes
	XPRINTF buf,mform,4
	lea	buf,A0
	moveq	#GREEN,D0
	bra	Status

 IFD GER
mform	DC.B "Freier Speicher: %ld Bytes."
 ELSE
mform	DC.B "Free memory: %ld bytes."
 ENDC
	DC.B 0
	EVEN

; **************************************************************


 IFEQ SOLO
CheckSerial:
	movem.l	d0-d7/a0-a6,-(sp)
	moveq	#0,d0
	lea	seroffs(pc),a3
	move.w	(a3),d0
	add.l	d0,a3
	bsr	Dec2Bin
	bsr	recalcser
	move.l	sernum,d1
	cmp.l	d0,d1	
	beq.s	.ok
	lsl.w	#2,d1
	jmp	(a3,d1.w)
.ok	moveq	#-1,d0
	movem.l	(sp)+,d0-d7/a0-a6
	rts

	
recalcser:
	eor.l	#$9aa90,d0		;coded -> 1-100
	eor.l	#$ddead,d0
	add.l	#25,d0
	divu	#9999,d0
	and.l	#$ffff,d0
	sub.l	#23,d0
	rts	

calcser:
	add.l	#23,d0			;1-100 -> coded
	mulu	#9999,d0
	sub.l	#25,d0
	eor.l	#$9aa90,d0
	eor.l	#$ddead,d0
	rts
 ENDC
	
;	*** ShowWindow(windowstruct)
;	***                 a0
;	*** ------------------------
ShowWindow:
	bsr	x_OpenWindow	; in D0 Zeiger auf Puffer
	bsr	mouseclick 
	RestoreWindow D0
	rts

;	*** ShowAbout
;	*** ---------
ShowAbout:
	moveq	#12,d0
	moveq	#9,d1
	moveq	#12+46,d2
	moveq	#9+22,d3
	moveq	#LGREY,d4
	movem.w	d0-d3,-(sp)
	bsr	x_Box
	bsr	ReleaseLMB
	movem.w	(sp)+,d0-d3
	moveq	#BLACK,d4
	bsr	x_Box
	bsr	ShowFreeMem
	lea	w_About(pc),a0
	bra	ShowWindow
	
;	*** ShowCoders
;	*** ----------
ShowCoders:
	;
	move.w	#260,d0
	moveq	#9,d1
	move.w	#260+46,d2
	moveq	#9+22,d3
	moveq	#LGREY,d4
	movem.w	d0-d3,-(sp)
	bsr	x_Box
	bsr	ReleaseLMB
	movem.w	(sp)+,d0-d3
	moveq	#BLACK,d4
	bsr	x_Box
	bsr	ShowFreeMem
	lea	w_Serial(pc),a0
	bra	ShowWindow

TCOL	SET RED

w_About:
	WINDOW 72,60,168,80,31,9,0,0,.wpyr
DF	SET SHADOW!BACKGND
.wpyr	DC.W	7-1
POSN SET 4
	TXT_GADGET 12,POSN,YELLOW,DF,<"COPYRIGHT ASI 1992,">
POSN SET POSN+9
	TXT_GADGET 12,POSN,YELLOW,DF,<"ALL RIGHTS RESERVED">
POSN SET POSN+13
	TXT_GADGET 28,POSN,TCOL,DF,<"ASI, THE VALLEY">
POSN SET POSN+9
	TXT_GADGET 36,POSN,TCOL,DF,<"ANGUILLA, BWI">
POSN SET POSN+14
	TXT_GADGET 28,POSN,YELLOW,DF,<"DISTRIBUTION BY">
POSN SET POSN+11
	TXT_GADGET 4,POSN,TCOL,DF,<"CACHET, OSTENDSTR.32">
POSN SET POSN+9
	TXT_GADGET 20,POSN,TCOL,DF,<"D-7524 OESTRINGEN">

 IFEQ SOLO
w_Serial:
	WINDOW 80,60,160,80,31,9,0,0,.wpyr
DF	SET SHADOW!BACKGND
.wpyr	DC.W	7-1
POSN SET 4
	TXT_GADGET 4+6*8,POSN,YELLOW,DF,<"THE REAL">
POSN SET POSN+11
	TXT_GADGET 4,POSN,YELLOW,DF,<"X-COPY PROFESSIONAL">
POSN SET POSN+13
	TXT_GADGET 4,POSN,TCOL,DF,<"  SEPTEMBER 1992">
POSN SET POSN+9
serialtxt:
	TXT_GADGET 4,POSN,TCOL,DF,<"  DISTRIBUTION BY   ">
POSN SET POSN+13
	TXT_GADGET 20,POSN,YELLOW,DF,<"CACHET SOFTWARE">
POSN SET POSN+11
	TXT_GADGET 12,POSN,TCOL,DF,<"TEL: 0725 322 411">
POSN SET POSN+8
	TXT_GADGET 12,POSN,TCOL,DF,<"FAX: 0725 322 450">
 ELSE
w_Serial:
	WINDOW 80,60,160,80,31,9,0,0,.wpyr
DF	SET SHADOW!BACKGND
.wpyr	DC.W	7-1
POSN SET 4
	TXT_GADGET 4+6*8,POSN,YELLOW,DF,<"THE REAL">
POSN SET POSN+11
	TXT_GADGET 4,POSN,YELLOW,DF,<"X-COPY PROFESSIONAL">
POSN SET POSN+13
	TXT_GADGET 4,POSN,TCOL,DF,<"  SEPTEMBER 1992">
POSN SET POSN+13
	TXT_GADGET 20,POSN,YELLOW,DF,<"CACHET SOFTWARE">
POSN SET POSN+11
	TXT_GADGET 12,POSN,TCOL,DF,<"TEL: 0725 322 411">
POSN SET POSN+8
	TXT_GADGET 12,POSN,TCOL,DF,<"FAX: 0725 322 450">
POSN SET POSN+9
	TXT_GADGET 52,POSN,TCOL,DF,<"GERMANY">

 ENDC

	CODESEG

 IFNE SOLO

FUNC_LINES = 10
FUNC_SIZE = 24
WILD_LINES = 80
WILD_SIZE = 16

CreateCopper:
	;
	; *** FUNCTION BUTTONS COPPER
	;
	move.w	#$2c+81,d0
	lea	funccopper,a1
	moveq	#FUNC_LINES-1,d2	;anzahl der zeilen
	;
.floop	move.w	d0,d1			;copper wait position
	lsl.w	#8,d1
	or.w	#$0f,d1
	move.w	d1,(a1)+
	move.w	#$fffe,(a1)+		;copper wait mask
	move.w	#$186,(a1)+		;color register 1
	move.w	#$fd0,(a1)+		;color 1
	;
	move.b	#$0085,d1		;horizontal wait 1
	move.w	d1,(a1)+
	move.w	#$fffe,(a1)+		;horizontal wait mask
	move.w	#$186,(a1)+		;color reg
	move.w	#$2b2,(a1)+
	;
	move.b	#$00C5,d1		;horizontal wait 2
	move.w	d1,(a1)+
	move.w	#$fffe,(a1)+		;horizontal wait mask
	move.w	#$186,(a1)+		;color reg
	move.w	#$d00,(a1)+
	;
	addq.w	#1,d0
	dbf	d2,.floop
	;
	; *** TRACKDISPLAY COPPER
	;
	move.w	#$2c+102,d0		;startcopperline
	lea	wildcopper,a1		;copperlist destination
	moveq	#WILD_LINES-1,d2	;anzahl der zeilen
	;
.wloop	move.w	d0,d1			;copper wait position
	lsl.w	#8,d1
	or.w	#$0f,d1
	move.w	d1,(a1)+
	move.w	#$fffe,(a1)+		;copper wait mask
	move.w	#$184,(a1)+		;color register 2
	move.w	#$d60,(a1)+		;color 2
	;
	move.b	#$0085,d1		;horizontal wait
	move.w	d1,(a1)+
	move.w	#$fffe,(a1)+		;horizontal wait mask
	move.w	#$184,(a1)+		;color reg
	move.w	#$2a2,(a1)+
	;
	addq.w	#1,d0
	dbf	d2,.wloop
	;
	rts

 ENDC

	CODESEG
	EVEN

 IFEQ SOLO
;	*** KILLSYSTEM (KillSys)
;	***----------------------
KILLSYSTEM:
	move.l	4,a0		
	move.l	62(a0),maxchip
	
	lea	w_ksys,A0
	bsr	x_OpenWindow
	move.l	D0,-(SP)	; Windowpointer speichern
	lea	w_ksys,A0
	bsr	x_CheckGadgets
	move.w	D0,D6	; Rueckgabewert speichern
	bsr	ReleaseLMB
	RestoreWindow (SP)+
	cmp.w	#1,D6
	bne	return	; es ist nicht OK angeklickt worden
	
	bsr	DrivesOFF
	bsr	CheckFastMem
	movem.l	default,d1-d7/a1-a2; alle wichtigen Werte an SOLO uebergeben	
	movem.l	d1-d7/a1-a2,defstore
	move.w	#$7fff,D0
	move.w	D0,$DFF096
	move.w	D0,$DFF09C
	move.w	D0,$DFF09A

	move.l	#SOLOSTART-8,A1	; Destination ($3F8)
;	lea	end_of_menue,a0
;	lea	end_of_solo,a2
;.copy	move.b	(a0)+,(a1)+
;	cmp.l	a0,a2
;	bne.s	.copy

	lea	tmpstackend,a7
	lea	solodata,a0	; Source
	lea	SOLOSTART-8,a1	; destination
	bsr	PPDecrunch

; 	bsr	CheckSerial
	movem.l	defstore,d1-d7/a1-a2
;	move.l	#$FC0002,D7	; Ruecksprungadresse (fällt aus!)
	jmp	SOLOSTART-8

defstore:	DS.L 18
tmpstack:	DS.B 128
tmpstackend:

	ENDB	a4
	INCLUDE	"depack.s"
	BASEREG	VarBasis+$8000,a4


 ENDC

 IFEQ SOLO

LN_TYPE = 12
MH_LOWER = 20
MH_UPPER = 24
LN_SUCC = 0
MemList = 322
TypeOfMem = -534
MEMB_FAST = 2

;	*** CheckFastMem - Determine first FAST-MEMORY board address
;	***---------------------------------------------------------
CheckFastMem:
	lea	lofast,a0
	clr.l	(a0)+
	clr.l	(a0)
	move.l	4,a6
	lea	MemList(a6),a2
	;
.loop	move.l	LN_SUCC(a2),a2	
	tst.l	(a2)
	beq.s	.done
	move.l	MH_LOWER(a2),a1		;get lower memory address
	jsr	TypeOfMem(a6)		;type of mem
	btst	#MEMB_FAST,d0		;fastmem ?
	beq	.loop
	lea	lofast,a0
	move.l	MH_LOWER(a2),(a0)+
	move.l	MH_UPPER(a2),(a0)
.done	rts

 ENDC
	INCLUDE "xcop.s"

; **************************************************************
;
; folgende Daten muessen im CHIPMEM liegen

	DATASEG
	EVEN

sernum	DC.L $deaddead		;serial number 1-100
 IFEQ SOLO
BUFFERPOOL			; hier liegen waehrend des Programablaufes
xcopypic:
  IFD GER
	INCBIN	"gfx/xcopypic_d.pp"
  ELSE
	INCBIN	"gfx/xcopypic.pp"
  ENDC
xcopypicend:

end_of_menue			; Startadresse fuer Entpacker
solodata
	INCBIN	"solo.pp"
;	INCBIN	"solo51.400"
solodataend
end_of_solo

 ENDC

 IFNE SOLO

BUFFERPOOL	= $bc00	; Pufferbereich wird bei SOLO ans Programm,$c100
		; angehaengt
 ENDC

; direkt am Anfang vom BUFFERPOOL liegt eine Pointertabelle fuer
; die Speicherverwaltung des Ram-Copys (984 Bytes)

CHARBUF		= BUFFERPOOL+984	; CHARBUF  ist 320 Bytes gross
CHARMASK	= CHARBUF+320		; CHARMASK  "  640   "     "

WORKTAB		= CHARMASK+640	; insgesamt 300 Bytes
;coutab	= lentab+$40	; 28*2=56 Rest ist Reserve
;syncpos 	= coutab+$80	; 28*4=112

MSKSTO		= CHARMASK+640+300	; 300 Bytes sind fuer die WorkTab reserviert

	EVEN

SOUNDDATA	DC.L	$7f7f8080

sprite:	DC.L $00000000,$f8000000,$9000E000,$9000E000,$8c00f000,$e200fc00
	DC.L $91009e00,$11001f00,$09000f00,$06000600,$00000000

normcols	DC.W $009e,$007d,$004b	; Mauszeiger BLUE (Farben 25-27)
freezecols	DC.W $0F0e,$0f0a,$0906 	; Mauszeiger ROSA fuer Freeze

coplist:
	DC.W $0120,$0000,$122,dumy,$0124,$0000,$0126,dumy
	DC.W $0128,$0000,$12A,dumy,$012C,$0000,$012E,dumy
	DC.W $0130,$0000,$132,dumy,$0134,$0000,$0136,dumy
	DC.W $0138,$0000,$13A,dumy,$013C,$0000,$013E,dumy
	DC.W $0100,$0200,$008E,$2c81,$0090,$f4C1,$0092,$0038
;	DC.W $0100,$0200,$008E,$0581,$0090,$40C1,$0092,$0038
	DC.W $0094,$00D0,$0102,$0000,$0104,$0024
	DC.W $0108,(DEP-1)*BPR,$010A,(DEP-1)*BPR
cop_planes
	DC.W $00E0,$0000,$00E2,$0000,$00E4,$0000,$00E6,$0000
	DC.W $00E8,$0000,$00EA,$0000,$00EC,$0000,$00EE,$0000
	DC.W $00F0,$0000,$00F2,$0000
	DC.W $2C01,$FFFE,$0100,((DEP)*$1000)+$200

 IFNE SOLO
cop_dummy:
	DC.W $1fe,0
	DC.W $180,$000,$182,$6aa,$184,$366,$186,$bdf


	DC.W $5901,$fffe,$186,$f72,$184,$eb0	; runde buttons auf gelb
	DC.W $6301,$fffe,$184,$366,$186,$bdf	; ...und wieder zurück

	DC.W $6a01,$fffe,$184,$fc0		; doscopy etc. gelb
	DC.W $7201,$fffe,$184,$366		; ...und wieder zurück

funccopper:
	DS.B FUNC_LINES*FUNC_SIZE
	DC.W $182,$6aa,$184,$366,$186,$bdf	; ...und wieder zurück
wildcopper:
	DS.B WILD_LINES*WILD_SIZE
	DC.W $182,$6aa,$184,$366,$186,$bdf	; ...und wieder zurück

	DC.W $E601,$FFFE,$184,$3d3,$182,$c00	; status message colors (green/red)
	DC.W $F101,$fffe,$184,$366,$182,$6aa	; ...und wieder zurück
 ELSE
	;*** COPPER VERLÄUFE FÜR DIE MESSAGES (H.V 18-9-91)
	DC.W	$6a01,$fffe,$19a,$fc0
	DC.W	$7201,$fffe,$19a,$292	;original

 ENDC
;	DC.W	$F401,$FFFE,$0100,$0200,$0180,$0000
	DC.W	$FFFF,$FFFE


	BSSSEG
mskpth		DS.L	1
vbl		DS.W	1
SYSTime		DS.L	1
TDly		DS.W	1	; Zeit nur einmal pro Sekunde ausgeben
blitter		DS.W	1
copy_active 	DS.W	1
dir_active	DS.W	1
mouseflag	DS.W	1
RMB_delay	DS.W	1
planeptr	DS.L	1
MemTab		DS.L	1
WorkTab		DS.L	1

; ** Variablen fuer StringOut

ZEILE		DS.W	1
BREITE		DS.W	1
MASK		DS.L	1
DRAWMAP		DS.L	1
DEPTH		DS.W	1
	DATASEG

MINTERM	DC.W	$0FCA

; ** Zeichensatz fuer Trackdisplay

zahl
 DC.L	$60000000,$90000000,$90000000,$90000000,$60000000
 DC.L	$60000000,$20000000,$20000000,$20000000,$20000000
 DC.L	$e0000000,$10000000,$60000000,$80000000,$f0000000
 DC.L	$e0000000,$10000000,$60000000,$10000000,$e0000000
 DC.L	$10000000,$30000000,$50000000,$f8000000,$10000000
 DC.L	$f0000000,$80000000,$e0000000,$10000000,$e0000000
 DC.L	$60000000,$80000000,$e0000000,$90000000,$60000000
 DC.L	$f0000000,$20000000,$40000000,$40000000,$40000000
 DC.L	$60000000,$90000000,$60000000,$90000000,$60000000
 DC.L	$60000000,$90000000,$70000000,$10000000,$60000000
 DC.L	$60000000,$90000000,$90000000,$f0000000,$90000000
 DC.L	$e0000000,$90000000,$e0000000,$90000000,$e0000000
 DC.L	$60000000,$b0000000,$80000000,$b0000000,$60000000
mask
 DC.L	$F8000000,$F8000000,$F8000000,$F8000000,$F8000000

; ** normaler Zeichensatz

charset INCBIN	"font.map"

 IFD XCOPYFONT
 DC.L	$186c6c,$18003818,$c300000,$3,$3c183c3c,$1c7e1c7e,$3c3c0000
 DC.L	$c00303c,$7c18fc3c,$f8fefe3c,$3c6c6c,$3ec66c18,$18186618,$6
 DC.L	$66386666,$3c603066,$66661818,$18001866,$c63c6666,$6c666666,$3c00fe
 DC.L	$60cc6830,$300c3c18,$c,$6e180606,$6c7c6006,$66661818,$307e0c06
 DC.L	$de3c66c0,$666060c0,$18006c,$3c187600,$300cff7e,$7e0018,$7e181c1c
 DC.L	$cc067c0c,$3c3e0000,$6000060c,$de667cc0,$667878ce,$1800fe,$630dc00
 DC.L	$300c3c18,$30,$76183006,$fe066618,$66060000,$30000c18,$de7e66c0
 DC.L	$666060c6,$6c,$7c66cc00,$18186618,$18001860,$66186666,$c666618
 DC.L	$660c1818,$187e1800,$c0c36666,$6c666066,$18006c,$18c67600,$c300000
 DC.L	$180018c0,$3c7e7e3c,$1e3c3c18,$3c381818,$c003018,$78c3fc3c,$f8fef03e
 DC.L	$0,$0,$0,$30000000,$0,$0,$30
 DC.L	$0,$0,$0,$667e0ee6,$f082c638,$fc38fc3c,$7e66c3c6
 DC.L	$c3c3fe3c,$c03c1000,$1800e000,$e001c00,$e01806e0,$38000000,$66180666
 DC.L	$60c6e66c,$666c6666,$5a66c3c6,$66c3c630,$600c3800,$18006000,$6003600
 DC.L	$60000060,$18000000,$6618066c,$60eef6c6,$66c66670,$186666c6,$3c668c30
 DC.L	$300c6c00,$c3c6c3c,$363c303b,$6c380666,$18667c3c,$7e180678,$60fedec6
 DC.L	$7cc67c38,$186666d6,$183c1830,$180cc600,$67666,$6e667866,$7618066c
 DC.L	$18776666,$6618666c,$62d6cec6,$60c66c0e,$18663cfe,$3c183230,$c0c0000
 DC.L	$1e6660,$667e3066,$66180678,$186b6666,$66186666,$66c6c66c,$606c6666
 DC.L	$18663cee,$66186630,$60c0000,$666666,$6660303c,$6618066c,$18636666
 DC.L	$667e3ce6,$fec6c638,$f03ce33c,$3c3e18c6,$c33cfe3c,$33c0000,$3b3c3c
 DC.L	$3b3c78c6,$e63c66e6,$3c63663c,$0,$0,$60000,$0
 DC.L	$0,$fe,$0,$7c,$3c00,$0,$0
 DC.L	$8000000,$e,$18707200,$0,$0,$0,$0
 DC.L	$0,$0,$0,$18000000,$18,$18189c00,$0
 DC.L	$0,$0,$0,$0,$0,$dc3dec3e,$3e666663
 DC.L	$63667e18,$18180000,$0,$0,$0,$0,$0
 DC.L	$0,$66667660,$1866666b,$36664c70,$180e0000,$0,$0
 DC.L	$0,$0,$0,$0,$6666663c,$1866666b,$1c661818
 DC.L	$18180000,$0,$0,$0,$0,$0,$0
 DC.L	$7c3e6006,$1a663c36,$363c3218,$18180000,$0,$0,$0
 DC.L	$0,$0,$0,$6006f07c,$c3b1836,$63187e0e,$18700000
 DC.L	$0,$0,$0,$0,$0,$0,$f0070000
 DC.L	$0,$700000,$0,$0,$0,$0,$0,$0,$0

 ENDC

;*******************

 IFEQ SOLO

Cols:
;	DC.W	$000,$68b,$7ac,$bdf	;solo!

	DC.L $00000366,$04780599,$06AA08BC,$09DD0BEF
	DC.L $000B004B,$007D009E,$03D20292,$0E200E60
	DC.L $0E700E80,$0E900EB0,$0FC00FF0,$073008A7
	DC.L $0FA8009E,$007D004B,$0F3C0F3C,$0E200507

;	DC.W	$0000,$0444,$0555,$0888,$0999,$0aaa,$0ccc,$0ddd
;	DC.W	$000b,$004b,$007d,$009e,$03f3,$0292,$0e20,$0e60
;	DC.W	$0e70,$0e80,$0e90,$0eb0,$0fc0,$0ff0,$0730,$08a7
;	DC.W	$0fa8,$009e,$007d,$004b	; Maus Farben
;	DC.W	$0f3c,$0f3c,$0f3c,$0507	; Fuer Copyright-Window


Icons:
 DC.L	$20,$c8,$170,$218,$25c,$2a0,$2ee,$44114411

;	*** Birne unselected ***
 DC.W	1,16	;words(width).W, height.W
 DC.L	0	;maskptr.L
 DC.L	$3a002200,$1c000000,$3c00,$30007f00
 DC.L	0,$fc80f880,$7f000000,$7c00,$7800ff80,$0,$7c003800
 DC.L	$ff800000,$3000,$ff80,$0,$80808080,$7f000000,$0
 DC.L	$7f00,$0,$41004100,$3e000000,$0,$3e00,$0
 DC.L	$24003600,$38000000,$1200,$36000e00,$0,$24003600,$38000000
 DC.L	$1200,$36000e00,$0,$24003600,$38000000,$1000,$14000c00,0

;	*** Birne selected ***
 DC.W	1,16
 DC.L	0
 DC.L	$20002000,$38002000,$1e007000,$43007c00
 DC.L	$40003f00,$f8009300,$fc008000,$6f807800,$a3807c00,$df80,$38008380
 DC.L	$7c000000,$ff804c00,$cf803000,$ff80,$7f00,$0,$ff800000
 DC.L	$7f000000,$7f00,$40007e00,$40004000,$3f000000,$3e000000,$3e00
 DC.L	$24003600,$38000000,$1200,$36000e00,$0,$24003600,$38000000
 DC.L	$1200,$36000e00,$0,$24003600,$38000000,$1000,$14000c00,0

;	*** Birne selected verify ***
 DC.W	1,16
 DC.L	0
 DC.L	$20002000,$38002000,$1e007100,$43007d00
 DC.L	$41003e00,$fb00f300,$ff00e300,$c805900,$e3807f00,$63009c80,$800b780
 DC.L	$7e003600,$c9806a00,$ff803600,$3600c980,$7f00,$1c001c00,$e3801400
 DC.L	$7f001c00,$1c006300,$48007e00,$48004800,$37000000,$3e000000,$3e00
 DC.L	$24003600,$38000000,$1200,$36000e00,$0,$24003600,$38000000
 DC.L	$1200,$36000e00,$0,$24003600,$38000000,$1000,$14000c00,0

;	*** Dreieck oben /\ ***
 DC.W	1,6
 DC.L	0
 DC.L	$800,$800,$0,$14000000
 DC.L	$14000000,$2200,$2200,$0,$41000000,$41000000,$8080
 DC.L	$8080,$0,$ff800000,$ff800000

;	*** Dreieck unten \/ ***
 DC.W	1,6
 DC.L	0
 DC.L	$ff80,$ff80,$0,$80800000,$80800000,$4100,$4100,$0
 DC.L	$22000000,$22000000,$1400,$1400,$0,$8000000,$8000000

;	*** Kreis (Buttons) O ***
 DC.W	1,7
 DC.L	0
 DC.L	$38003800,$0,$38004000,$4004000,$4400
 DC.L	$82008200,$2000200,$80008200,$82000200,$2008000,$2008200,$2000200
 DC.L	$80000400,$44000400,$4004000,$38003800,$38003800
 DC.W	0

;	*** Function Rechteck ***
 DC.W	3,9
 DC.L	0
 DC.W	$7fff
 DC.L	$fffffe00,$7fffffff,$fe007fff,$fffffe00,$0,$0
 DC.L	$0,$80000000,$1008000,$100,$80000000,$1000000,$0
 DC.L	$0,$8000,$100,$80000000,$1008000,$100,$0
 DC.L	$0,$0,$80000000,$1008000,$100,$80000000,$1000000
 DC.L	$0,$0,$8000,$100,$80000000,$1008000,$100
 DC.L	$0,$0,$0,$80000000,$1008000,$100,$80000000
 DC.L	$1000000,$0,$0,$8000,$100,$80000000,$1008000
 DC.L	$100,$0,$0,$0,$80000000,$1008000,$100
 DC.L	$80000000,$1000000,$0,$0,$7fff,$fffffe00,$7fffffff
 DC.L	$fe007fff,$fffffe00,$0,$0,$0

 ENDC

;**************

 IFNE SOLO


BitMAP:
 IFD GER
	INCBIN	"gfx/solopic_d.raw"
 ELSE
	INCBIN	"gfx/solopic.raw"
 ENDC
	EVEN
Cols:	DC.W	$000,$68b,$7ac,$bdf

;	***----------------
;	*** SoloCopy Icons
;	***----------------
	;
;	*** MACRO: XICON wordwidth,pixelheight,mask,filename
XICON	MACRO
	DC.W \1,\2
	DC.L \3
	INCBIN \4
	ENDM
	;
Icons:
	DC.L .birne1-Icons,.birne2-Icons,.birne3-Icons
	DC.L .tri1-Icons,.tri2-Icons,.circle-Icons,.square-Icons
	DC.L $44114411
	;
.birne1	XICON 1,16,0,<"gfx/birne1.raw">
.birne2	XICON 1,16,0,<"gfx/birne2.raw">
.birne3	XICON 1,16,0,<"gfx/birne3.raw">
.tri1	XICON 1,6,0,<"gfx/tri1.raw">
.tri2	XICON 1,6,0,<"gfx/tri2.raw">
.circle	XICON 1,7,0,<"gfx/circle.raw">
.square	XICON 3,9,0,<"gfx/square.raw">


 DC.L	$20,$88,$f0,$158,$184,$1b0,$1e2
 DC.L	$44114411,$10010,$0,$3e003e00,$4c00,$4f003000,$84808780
 DC.L	$78000400,$87807800,$4400c780,$38003000,$ff800000,$ff80,$8080
 DC.L	$ff800000,$7f00,$4100,$7f000000,$e00,$30000000,$38000600
 DC.L	$e00,$30000000,$38000600,$e00,$30000000,$18000400,$10010
 DC.L	$0,$3e002600,$7f00,$40000000,$ef808080,$1000cf80,$3000
 DC.L	$ff800000,$ff80,$0,$ff808080,$7f00,$0,$7f004100
 DC.L	$0,$3e000000,$e00,$30000000,$38000600,$e00,$30000000
 DC.L	$38000600,$e00,$30000000,$18000400,$10010,$0,$3e002600
 DC.L	$7f00,$41000000,$8c80e380,$73008c80,$63007300,$c9803600,$3600c980
 DC.L	$36003600,$e3809c80,$1c006300,$1c001c00,$77004900,$8000000,$3e000000
 DC.L	$e00,$30000000,$38000600,$e00,$30000000,$38000600,$e00
 DC.L	$30000000,$18000400,$10006,$0,$8000800,$8001400,$14001400
 DC.L	$22002200,$22004100,$41004100,$80808080,$8080ff80,$ff80ff80,$10006
 DC.L	$0,$ff80ff80,$ff808080,$80808080,$41004100,$41002200,$22002200
 DC.L	$14001400,$14000800,$8000800,$10007,$0,$3800,$0
 DC.L	$44000000,$2008200,$200,$82000000,$2008200,$400,$44000000
 DC.L	$38003800,$3,$90000,$0,$0,$0,$7fff
 DC.L	$fffffe00,$0,$0,$0,$80000000,$1000000,$0
 DC.L	$0,$8000,$100,$0,$0,$0,$80000000
 DC.L	$1000000,$0,$0,$8000,$100,$0,$0
 DC.L	$0,$80000000,$1000000,$0,$0,$8000,$100
 DC.L	$0,$0,$0,$80000000,$1000000,$0,$0
 DC.L	$7fff,$fffffe00


 ENDC


;*******************
	END
