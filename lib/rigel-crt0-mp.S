# TODO Add descriptive comment of what is meant to reside in this file

.equ STACKSIZESHIFT, (18) #; 256kB per Thread (for up to 4-way MT)

# TODO FIXME: down below, corenum is hardcoded in to an immediate!

#DO NOT USE R4, R5, or R6 in this code!  They are set by the simulator at the beginning of the
#simulation to argc, argv, and env.  This function will pass those on to __start(), then to main()

.section .text
.set mipsrigel32
.globl _start
_start:
	nop
	mfsr	$2, $6 #R2 gets the threadnum

	#; ...adjust stack approprately
	ori		$1, $0, 1
	slli  $1, $1, STACKSIZESHIFT #R1 = 1 << STACKSIZESHIFT
	mul		$2, $1, $2 #r2 = threadnum * (1 << STACKSIZESHIFT)
	sub		$sp, $2, $0 # sp = 0 - (threadnum * (1 << STACKSIZESHIFT))
  #So thread 0 has the highest-numbered stack addresses, then thread 1, etc.
  #Write this thread's lowest stack address to the SPRF.
  #The simulator uses this to track stack vs. heap vs. code reads/writes, do stack-smash checking, etc.
  sub   $3, $1, $sp
  mtsr  $15, $3

	#; To prevent collisions in the cluster cache on stack data, we skew the
	#; stack	pointer a bit
	mfsr	$2, $0          #; ($2<-CORENUM)
  mfsr  $3, $4          #; ($3<-CLUSTERNUM)
  andi  $3, $3, 0x0F    #; Limit the number of lines to skip.  Based on tile size = 16
  addi  $8, $0, (32*7*8)#; $4 <- (Amount of skewing per cluster)
  mul   $3, $3, $8
	addi	$1, $0, (32*7)	#; ($1<-32*7)  Off by 7 cache lines for good measure 
	andi	$2, $2, 0x0007	#; ($2<-CORENUM & 0x7) - Get local core number, FIXME: this is hard-coded, BAD!
	mul		$2, $1, $2			#; ($2<-$2*$1) - Set offset to 32*7*local_core_num	
  add   $2, $2, $3      #; Add in per cluster offset.
	sub		$sp, $2, $sp    #; sp<-($sp-$2) ???
#;  printreg	$sp

	#; Clean up
	xor		$1, $1, $1
	xor		$2, $2, $2
	xor		$3, $3, $3

  # ARGC, ARGV
  # NOTE: Now, argc, argv, and env (not env?) are set by the simulator
  #       SPRF values hold argv and argc, here we move them into R4,R5
  mfsr $4, $17  #; argc
  mfsr $5, $18  #; argv

	mvui $8, %hi(__start)
	addi	$8, $8, __start
	jalr $8
	#;jal $31, __start;

#; Halt doesn't always work the first time in the simulator,
#; so we use a whole bunch of them.  FIXME!
_halt:
	hlt;
  beq $0, $0, _halt
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
  nop;
#; If this ever gets executed, something *very* bad happened.
	hlt;
	hlt;
	hlt;
	hlt;
  abort;
  nop;
