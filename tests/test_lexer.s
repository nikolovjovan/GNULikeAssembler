.data
test: .skip 100, 0xff
test2: .skip 0x43, 057
.skip 0b10110110

.equ num, 45

.text
.align 2, 057, 	 1 # first - alignment in B, second - fill value (0 = default - NOP), third - max fill in B (if more is needed then do not align)

.GLOBAL MaIn
.eXtERN printf, test, 		s2areage._est  # comment parsing

   MaIn:    # 		test comment
test3: mov	r0, &num
	add r5, [r2]
	test r2[0x5], r0
	subb r2l, r3h
	xchgw r0, sp
	halt

	.data
	n:  .word 0x195f
 TESTMatch:  .word 0x195f	   #comment 123
   .L0: # gcc style labels
   _testLab_el12.3test: #mov r0, r1#test
#	023test:		    # invalid label starts with digit
#	t$test:				 #invalid label contains $
	test_:   

.section 		 testsection	 , 	"bw" # flags, see asm.h
# .section 		 testsection2	 , 	"brrwd" # double r should error

.end

junkdata: .word 0x54 # this wont be visible