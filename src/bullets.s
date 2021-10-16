MAX_BULLETS = 32

.segment "ZEROPAGE"

num_bullets: .res 1

.segment "BSS"

.export _bullets_x, _bullets_sx, _bullets_y, _bullets_sy
.export _bullets_delta_x, _bullets_delta_sx, _bullets_delta_y, _bullets_delta_sy
.export _bullets_type

_bullets_x: .res MAX_BULLETS
_bullets_sx: .res MAX_BULLETS
_bullets_y: .res MAX_BULLETS
_bullets_sy: .res MAX_BULLETS
_bullets_type: .res MAX_BULLETS
_bullets_delta_x: .res MAX_BULLETS
_bullets_delta_sx: .res MAX_BULLETS
_bullets_delta_y: .res MAX_BULLETS
_bullets_delta_sy: .res MAX_BULLETS

.segment "CODE"

.export _delete_bullet
.proc _delete_bullet
  TAX
  DEC num_bullets
  LDY num_bullets

  LDA _bullets_x, Y
  STA _bullets_x, X
  LDA _bullets_sx, Y
  STA _bullets_sx, X
  LDA _bullets_y, Y
  STA _bullets_y, X
  LDA _bullets_sy, Y
  STA _bullets_sy, X

  LDA _bullets_type, Y
  STA _bullets_type, X

  LDA _bullets_delta_x, Y
  STA _bullets_delta_x, X
  LDA _bullets_delta_sx, Y
  STA _bullets_delta_sx, X
  LDA _bullets_delta_y, Y
  STA _bullets_delta_y, X
  LDA _bullets_delta_sy, Y
  STA _bullets_delta_sy, X
  RTS
.endproc

.export _get_num_bullets
.proc _get_num_bullets
  LDA num_bullets
  LDX #$00
  RTS
.endproc

.export _inc_bullets
.proc _inc_bullets
  INC num_bullets
  RTS
.endproc

.export _reset_bullets
.proc _reset_bullets
  LDA #$00
  STA num_bullets
  RTS
.endproc
