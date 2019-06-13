.data
test: .skip 100, 0xff
.align 8, 03, 6
test2: .skip 0x43, 057
.skip 0b10110110
.byte   ~0b00100010, 		  074, 	 -135, 		0xf2
.word 		-0b0101010101010101,  	~0356,   	6421, 	0x3f2a
# .word 		~56,  	031  ,   	65536	 , 	0x3f2a # value 65536 is larger than a word value

.equ num, 45

.text
.align 2, 057, 	 1 # first - alignment in B, second - fill value (0 = default - NOP), third - max fill in B (if more is needed then do not align)

#.GLOBAL MaIn g++ 6.2 which is the latest one that can be used in Ubuntu 12.04
#             has inconsistent behaviour for the regex::icase flag, therefore
#             all directives/instructions/operands/etc. are now case-sensitive!
.global MaIn
#.eXtERN printf, test, 		s2areage._est  # comment parsing
.extern printf, test, 		s2areage._est  # comment parsing

   MaIn:    # 		test comment
test3: mov	r0, &num	# 1 + 1 + 3 = 5B
	add r5, [r2]		# 1 + 1 + 1 = 3B
	test r2[0x5], r0	# 1 + 2 + 1 = 4B
	and r4[536], r0		# 1 + 3 + 1 = 5B
	subb r2l, r3h		# 1 + 1 + 1 = 3B
	xchgw r0, sp		# 1 + 1 + 1 = 3B
	halt				# 1         = 1B

	.data
	n:  .word 0x195f
 TESTMatch:  .word 0x195f	   #comment 123
   .L0: # gcc style labels
   _testLab_el12.3test: #mov r0, r1#test
#	023test:		    # invalid label starts with digit
#	t$test:				 #invalid label contains $
	test_:   

.section	  	.rodata	 , 		"a" # .rodata is SHF_ALLOC but not SHF_WRITE
# .section 		 testsection2	 , 	"awwx" # double w should error

.end

junkdata: .word 0x54 # this wont be visible