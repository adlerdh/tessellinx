# tessellinx
Tile tessellation using the Dancing Links algorithm

Samples:

```
./tessellinx --unique-solutions --board-width 8 --board-height 8 --pieces=iq --print --video --video-width 480 --video-height 480 --video-fps 60 --video-file iq_blocks.mp4

./tessellinx --unique-solutions --board-width 8 --board-height 8 --board-mask ../boards/mask_8x8_2x2_hole.txt --pieces=pentominoes --print --video --video-width 480 --video-height 480 --video-fps 10 --video-file pentominoes_8x8_2x2_hole.mp4

./tessellinx --unique-solutions --board-width 10 --board-height 6 --pieces=pentominoes --print --video --video-width 800 --video-height 480 --video-fps 30 --video-file pentominoes_10x6.mp4

./tessellinx --unique-solutions --board-width 12 --board-height 5 --pieces=pentominoes --print --video --video-width 120 --video-height 50 --video-fps 10 --video-file pentominoes_12x5.mp4

./tessellinx --unique-solutions --board-width 15 --board-height 4 --pieces=pentominoes --print --video --video-width 150 --video-height 40 --video-fps 10 --video-file pentominoes_15x4.mp4

# This one fails:
./tessellinx --unique-solutions --board-width 20 --board-height 3 --pieces=pentominoes --print --video --video-width 200 --video-height 30 --video-fps 10 --video-file pentominoes_20x3.mp4

./tessellinx --unique-solutions --board-width 5 --board-height 5 --pieces-board ../boards/test_board.txt --print --video --video-width 50 --video-height 50 --video-fps 10 --video-file test_board.mp4

./tessellinx --unique-solutions --pieces-board ../boards/test_long_board.txt --print

# This one has many solutions:
./tessellinx --board-width 4 --board-height 14 --pieces=tetrominoes --print --video --video-width 480 --video-height 1680 --video-fps 60 --video-file tetrominoes_4x14.mp4

# No solutions:
./tessellinx --unique-solutions --board-width 13 --board-height 7 --board-mask ../boards/mask_heart_60_b.txt --pieces=pentominoes --print --video --video-width 520 --video-height 280 --video-fps 10 --video-file heart_60_a.mp4

./tessellinx --unique-solutions --board-width 19 --board-height 14 --board-mask ../boards/mask_heart_120.txt --pieces=pentominoes --print --video --video-width 190 --video-height 140 --video-fps 10 --video-file heart_120.mp4
```

Copyright (c) 2026 Daniel H. Adler. All rights reserved.
