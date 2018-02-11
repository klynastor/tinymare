" Vim syntax file
" Language:	TinyMARE Helptext file
" Maintainer:	Byron Stanoszek (gandalf@winds.org)
" Last Change:	2003 May 14

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

if &background == "dark"
  hi Quote	term=bold ctermfg=White guifg=#ffffff gui=bold
  hi Example	ctermfg=DarkCyan guifg=#005555
else
  hi Quote	term=bold ctermfg=Black guifg=#000000 gui=bold
  hi Example	ctermfg=DarkCyan guifg=#005555
endif

" These words are not hilited:
syn keyword mareNoKeyword contained  ASCII NOMOD
syn keyword mareNoKeyword contained  MAXHP MAXMP MAXEN CHANT SKILL FUMBLE REGEN
syn keyword mareNoKeyword contained  DIVISOR ALIGN HPLOSS MPLOSS ENLOSS NTARGETS
syn keyword mareNoKeyword contained  STEPS VAULT KILLS VIGOR EVADE MAXIP MOVES
syn keyword mareNoKeyword contained  MAXMOVES DRAIN

" Page Headers
if version < 600
  syn match mareEntry		"^&.*" nextgroup=mareHeader skipwhite skipempty
else
  syn match mareEntry		"^&.*\n\+" nextgroup=mareHeader
endif

syn match mareHeader		".*" contained nextgroup=mareHeader skipnl
syn match mareHeader		".* .* .* .*" contained transparent
syn match mareHeader		"^&.*" contained contains=mareEntry nextgroup=mareHeader
syn match mareHeader		"^[0-9a-z]\+(.*" contained nextgroup=mareHeader skipnl
syn match mareHeader		"^\S.*>$" contained nextgroup=mareHeader skipnl
syn match mareHeader		"^\s*\e.*" contained transparent
syn match mareHeader		"^\s*$" contained transparent

" Section Headers
syn match mareType		"^[0-9A-Z].*:.*"
syn match mareType		"³"
syn match mareHeadline		"^[0-9A-Z].*:$"
syn match mareSeeAlso		"^See also:" nextgroup=mareNoColor

" Document hiliting
syn region mareQuote		start=+^"+ end=+"\|^\s*$+
syn region mareQuote		start=+^'+ end=+'\|^\s*$+
syn region mareQuote		start=+[^0-9A-Za-z_]"\S+ms=s+1 end=+"\|^\s*$+
syn region mareQuote		start=+[^0-9A-Za-z_>}\]]'\S+ms=s+1 end=+'\|^\s*$+

" Hilite capital words > 4 letters
syn match mareConfig		"\<[A-Z][A-Z_]\{3,\}[A-Z]\>" contains=mareNoKeyword

" Command and Function examples
syn match mareCommand		"^\s*> .*$" nextgroup=mareExample skipnl
syn match mareFunctionExample	".* => " contains=mareYield nextgroup=mareExample skipnl
syn match mareYield		" => " contained

syn match mareExample		".*" nextgroup=mareExample contained contains=mareCommand,mareFunctionExample,marePronoun skipwhite skipnl
syn match mareExample		"^\s*$" contained transparent

" %-Pronouns and [function()] literals
syn match marePronoun		"%|.*|\|%[Vv][A-Za-z]\|%[0-9AaBbEeGgLlNnOoPpRrSsTtUuXx!#$?]"
syn region mareFunction		start='\[' end='\]\|^\s*$' contains=mareFunction

syn match mareNoColor		"\[ *\]"
syn match mareNoColor		".*" contained
syn match mareNoColor		"^\s*\e.*"

syn match Error			"\t"

if !exists("did_mare_syntax_inits")
  let did_mare_syntax_inits = 1

  syn sync minlines=8

  hi link mareEntry		Identifier
  hi link mareHeader		Special
  hi link mareHeadline		Statement
  hi link mareSeeAlso		String
  hi link mareSet		Comment
  hi link mareType		Comment

  hi link mareQuote		Quote
  hi link marePronoun		Quote
  hi link mareYield		Quote
  hi link mareConfig		Quote
  hi link mareVariable		Quote
  hi link mareFunction		Quote
  hi link mareFunctionExample	Example
  hi link mareCommand		Example
  hi link mareExample		Type
endif

let b:current_syntax = "mare"
