" GNU Libc printf %-substitution
syn clear	cFormat
syn match	cFormat		display "%\(\d\+\$\)\=[-+'#0*]*\(\d*\|\*\|\*\d\+\$\)\(\.\(\d*\|\*\|\*\d\+\$\)\)\=\([hlLqjzt]\|ll\|hh\)\=\([aAbdiuoxXDOUfFeEgGcCsSpn]\|\[\^\=.[^]]*\]\)"
syn match	cFormat		display "%%" contained

" GNU ({ }) style program block
syn clear	cErrInParen
syn match	cErrInParen	display contained "[\]]\|<%\|%>"

" Bit-values
syn match	cNumbers	display transparent "\<BV\d" contains=cNumber
syn match	cNumber		display contained "BV\d\+\>"
syn match	cNumber		display contained "0b[01]\+\(u\=l\{0,2}\|ll\=u\)\>"

" Additional keywords
syn keyword	cTodo		contained FIX WTF
syn keyword	cOperator	offsetof
syn keyword	cType		sigset_t fd_set
syn keyword	cConstant	NAN INFINITY
syn keyword	cConstant	EHOSTDOWN EPROTO

syn keyword	cType		optional interrupt
syn keyword	cType		dbref quad _Bool
syn keyword	cType		RAND LOC OBJ
syn keyword	cType		ATTR DESC CONN ALIST ATRDEF hash eqtech eqlist
syn keyword	cType		eqitem eqslot eqcond eqspell eqtactics
syn keyword	cType		eqmonster eqmoney
syn keyword	cType		u8 u16 u24 u32 u64 s8 s16 s24 s32 s64 ptr32
syn keyword	cType		align2 align4 align8 align16
syn keyword	cType		__int24 __uint24
syn keyword	cType		__flash __flash1 __flash2 __flash3 __flash4
syn keyword	cType		__flash5 __memx

syn keyword	cConstant	SIGCLD NOTHING AMBIGUOUS HOME MONEY GOD
