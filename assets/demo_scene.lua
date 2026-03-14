time = 0.0
target_x = 980.0
target_y = 360.0
sound_timer = 0.0
logged_startup = false

function update(dt)
    if not logged_startup then
        log_message("Lua scene script active")
        logged_startup = true
    end

    time = time + dt
    sound_timer = sound_timer + dt
    target_x = 980.0 + math.sin(time * 1.4) * 120.0
    target_y = 360.0 + math.cos(time * 2.1) * 48.0

    if sound_timer >= 3.0 then
        request_sound("test_tone")
        sound_timer = 0.0
    end
end
