--!nonstrict

function update()
  barWidget.setGlyph("photo")
  barWidget.setText("")
  barWidget.setTooltip("Wallpaper Engine")
end

function onClick()
  noctalia.runAsync([[hyprctl dispatch 'hl.dsp.exec_cmd("orbit-wallpaper-settings", { float = true, size = { 560, 760 }, move = { "monitor_w-560-20", 45 } })']])
end

update()
