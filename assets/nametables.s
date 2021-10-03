.segment "RODATA"

.export _title_nametable
.export _enemy_formations
.export _empty_nametable

_enemy_formations: .word _enemy_formation_1

_title_nametable: .incbin "nametables/title.rle"
_enemy_formation_1: .incbin "nametables/enemy1.rle"
_empty_nametable: .incbin "nametables/empty.rle"
