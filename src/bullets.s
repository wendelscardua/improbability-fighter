MAX_BULLETS = 32
OAM_BUF = $0200

.enum bullet_type
  PlayerBullet
  EnemyBullet
  PlayerApple
  PlayerBlock
.endenum

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

.import _oam_get, _oam_set

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

.export _draw_bullets
.proc _draw_bullets
  JSR _oam_get
  TAX

  LDY #$00
loop:
  CPY num_bullets
  BEQ exit_loop

  LDA _bullets_x, Y
  STA OAM_BUF+3, X
  LDA _bullets_y, Y
  STA OAM_BUF+0, X

  LDA _bullets_type, Y
  CMP #bullet_type::PlayerBullet
  BNE :+
  LDA #$00
  STA OAM_BUF+1, X
  LDA #$01
  STA OAM_BUF+2, X
  JMP next
: CMP #bullet_type::PlayerApple
  BNE :+
  LDA #$01
  STA OAM_BUF+1, X
  LDA #$02
  STA OAM_BUF+2, X
  JMP next
: CMP #bullet_type::EnemyBullet
  BNE :+
  LDA #$00
  STA OAM_BUF+1, X
  LDA #$02
  STA OAM_BUF+2, X
  JMP next
: CMP #bullet_type::PlayerBlock
  BNE :+
  LDA #$8a
  STA OAM_BUF+1, X
  LDA #$03
  STA OAM_BUF+2, X
  LDA _bullets_y, Y
  AND #$f8
  STA OAM_BUF+0, X
  JMP next
:

next:
  INY
  .repeat 4
    INX
  .endrepeat
  JMP loop
exit_loop:
  TXA
  JSR _oam_set
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

.export _set_bullet_x
.proc _set_bullet_x
  LDY num_bullets
  STA _bullets_sx, Y
  TXA
  STA _bullets_x, Y
  RTS
.endproc

.export _set_bullet_y
.proc _set_bullet_y
  LDY num_bullets
  STA _bullets_sy, Y
  TXA
  STA _bullets_y, Y
  RTS
.endproc

.export _set_bullet_delta_x
.proc _set_bullet_delta_x
  LDY num_bullets
  STA _bullets_delta_sx, Y
  TXA
  STA _bullets_delta_x, Y
  RTS
.endproc

.export _set_bullet_delta_y
.proc _set_bullet_delta_y
  LDY num_bullets
  STA _bullets_delta_sy, Y
  TXA
  STA _bullets_delta_y, Y
  RTS
.endproc

.export _set_bullet_type
.proc _set_bullet_type
  LDY num_bullets
  STA _bullets_type, Y
  RTS
.endproc

.export _update_bullets
.proc _update_bullets
  LDX #0
loop:
  CPX num_bullets
  BEQ exit_loop
  ; increment fixed-point coordinates
  CLC
  LDA _bullets_sx, X
  ADC _bullets_delta_sx, X
  STA _bullets_sx, X

  LDA _bullets_x, X
  ADC _bullets_delta_x, X
  STA _bullets_x, X

  CLC
  LDA _bullets_sy, X
  ADC _bullets_delta_sy, X
  STA _bullets_sy, X

  LDA _bullets_y, X
  ADC _bullets_delta_y, X
  STA _bullets_y, X

  ; special case for apples
  LDA _bullets_type, X
  CMP #bullet_type::PlayerApple
  BNE no_apple

  ; signed compare if delta y > -2
  LDA _bullets_delta_y, X
  SEC       ; prepare carry for SBC
  SBC #($100-$2)   ; A-NUM
  BVC :+ ; if V is 0, N eor V = N, otherwise N eor V = N eor 1
  EOR #$80  ; A = A eor $80, and N = N eor 1
:
  BMI no_apple

  ; delta speed - $00.10
  SEC
  LDA _bullets_delta_sy, X
  SBC #$10
  STA _bullets_delta_sy, X

  LDA _bullets_delta_y, X
  SBC #$00
  STA _bullets_delta_y, X
no_apple:

  ; delete bullet if out of bounds
  LDA _bullets_x, X
  CMP #$08
  BCC out_of_bounds
  CMP #$f8
  BCS out_of_bounds

  LDA _bullets_y, X
  CMP #$08
  BCC out_of_bounds
  CMP #$e0
  BCS out_of_bounds

  JMP next
out_of_bounds:
  TXA
  JSR _delete_bullet
  JMP loop
next:
  INX
  JMP loop
exit_loop:
  RTS
.endproc
