precision highp float;

uniform vec2 u_resolution;
uniform vec2 u_origin;
uniform vec2 u_canvas;
uniform float u_time;
uniform float u_brightness;
uniform float u_visibility;
uniform vec3 u_primary;
uniform vec3 u_secondary;
uniform vec3 u_surface;
uniform vec3 u_error;

vec3 saturate(vec3 color, float amount) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luminance), color, amount);
}

void main() {
    vec2 local = gl_FragCoord.xy / u_resolution;
    vec2 global = (u_origin + gl_FragCoord.xy) / u_canvas;
    float x = global.x;
    float y = global.y;

    float drift = u_time * 0.3;
    float wave = sin(x * 6.28318 * 1.12 + drift) * 0.12
               + sin(x * 6.28318 * 0.47 - drift * 0.61) * 0.04;
    float crest = 0.57 + wave;
    float distance_to_crest = y - crest;

    float upper = smoothstep(0.015, -0.005, distance_to_crest)
                * smoothstep(-0.105, -0.025, distance_to_crest);
    float lower = smoothstep(-0.012, 0.01, distance_to_crest)
                * smoothstep(0.145, 0.025, distance_to_crest);
    float rim = smoothstep(0.0045, 0.0, abs(distance_to_crest));
    float glow = smoothstep(0.2, 0.0, abs(distance_to_crest)) * 0.27;
    float side_glow = smoothstep(0.32, 0.0, abs(distance_to_crest)) * 0.09;

    // Low-opacity companion waves make the main ribbon feel suspended in motion.
    float drift_a = u_time * 0.4;
    float wave_a = sin(x * 6.28318 * 0.78 - drift_a + 1.8) * 0.055
                 + sin(x * 6.28318 * 1.9 + drift_a * 0.7) * 0.018;
    float wave_b = sin(x * 6.28318 * 0.54 + drift_a * 0.72 - 0.8) * 0.07;
    float wave_c = sin(x * 6.28318 * 1.55 - drift_a * 0.48 + 2.4) * 0.045;
    float companion_a = smoothstep(0.065, 0.0, abs(y - (0.42 + wave_a))) * 0.15;
    float companion_b = smoothstep(0.08, 0.0, abs(y - (0.68 + wave_b))) * 0.11;
    float companion_c = smoothstep(0.055, 0.0, abs(y - (0.77 + wave_c))) * 0.08;

    // Fine filaments drift through the crest and periodically cross its edge.
    float filament_a = sin(x * 6.28318 * 0.86 + drift * 0.76) * 0.048
                     + sin(x * 6.28318 * 2.2 - drift * 0.42) * 0.012;
    float filament_b = sin(x * 6.28318 * 0.58 - drift * 0.58 + 1.2) * 0.052;
    float filament_c = sin(x * 6.28318 * 1.34 + drift * 0.34 - 0.6) * 0.04;
    float razor_a = smoothstep(0.0018, 0.0, abs(distance_to_crest - 0.02 - filament_a));
    float razor_b = smoothstep(0.0014, 0.0, abs(distance_to_crest - 0.06 - filament_b));
    float razor_c = smoothstep(0.0011, 0.0, abs(distance_to_crest - 0.1 - filament_c));

    // A slow, submerged ribbon gives the lower half a second visual plane.
    float undertow_wave = sin(x * 6.28318 * 0.7 - drift * 0.42 + 2.0) * 0.075
                        + sin(x * 6.28318 * 1.6 + drift * 0.24) * 0.025;
    float undertow_center = 0.24 + undertow_wave;
    float undertow_distance = y - undertow_center;
    float undertow = smoothstep(0.16, 0.0, abs(undertow_distance)) * 0.16;
    float undertow_glow = smoothstep(0.24, 0.0, abs(undertow_distance)) * 0.1;
    float undertow_rim = smoothstep(0.004, 0.0, abs(undertow_distance + 0.018));
    float lower_filament = sin(x * 6.28318 * 1.1 + drift * 0.31) * 0.065;
    float lower_line = smoothstep(0.0017, 0.0,
        abs(y - (0.12 + lower_filament)));

    float deep_wave = sin(x * 6.28318 * 0.46 + drift * 0.18 - 1.1) * 0.04
                    + sin(x * 6.28318 * 1.25 - drift * 0.27) * 0.018;
    float deep_center = 0.075 + deep_wave;
    float deep_distance = y - deep_center;
    float deep_band = smoothstep(0.085, 0.0, abs(deep_distance)) * 0.1;
    float deep_glow = smoothstep(0.15, 0.0, abs(deep_distance)) * 0.065;
    float deep_rim = smoothstep(0.0025, 0.0, abs(deep_distance + 0.014));
    float lower_razor = sin(x * 6.28318 * 0.72 - drift * 0.22 + 0.8) * 0.055;
    float lower_razor_line = smoothstep(0.0014, 0.0,
        abs(y - (0.34 + lower_razor)));

    // Keep the surface dark, while making the wave colors richer.
    vec3 rich_primary = saturate(u_primary, 1.35);
    vec3 rich_secondary = saturate(u_secondary, 1.45);
    vec3 rich_surface = saturate(u_surface, 1.18);
    vec3 rich_error = saturate(u_error, 1.35);
    vec3 black = max(rich_surface * 0.2, vec3(0.003));
    vec3 dark_surface = max(rich_surface * 0.24, vec3(0.005));
    vec3 upper_color = mix(rich_primary, vec3(0.96), 0.42);
    vec3 lower_color = mix(rich_secondary, rich_surface, 0.36);
    vec3 accent = mix(rich_primary, rich_error, 0.28);

    float background_morph = 0.5 + 0.5 * sin(u_time * 0.015 + x * 2.4 + sin(y * 5.0));
    vec3 atmospheric_a = mix(dark_surface, rich_secondary * 0.16, background_morph);
    vec3 atmospheric_b = mix(rich_surface * 0.12, rich_primary * 0.07, 1.0 - background_morph);
    vec3 color = mix(black, atmospheric_a, smoothstep(0.0, 0.7, y) * 0.7);
    color += atmospheric_b * smoothstep(0.3, 1.0, y) * 0.55;
    color += mix(rich_secondary, rich_error, 0.35) * smoothstep(0.42, 0.0, y) * 0.08;
    color += upper_color * upper * (0.55 + 0.22 * sin(x * 10.0 - drift));
    color += lower_color * lower * (0.5 + 0.25 * (1.0 - y));
    color += accent * rim * 0.9;
    color += accent * glow;
    color += mix(rich_secondary, rich_error, 0.38) * side_glow;
    color += mix(rich_secondary, rich_primary, 0.35) * companion_a;
    color += mix(rich_primary, rich_secondary, 0.45) * companion_b;
    color += rich_secondary * companion_c;
    color += mix(rich_primary, vec3(1.0), 0.42) * (razor_a * 0.85 + razor_b * 0.72 + razor_c * 0.65);
    color += mix(rich_secondary, rich_surface, 0.25) * undertow * 0.8;
    color += mix(rich_secondary, rich_error, 0.35) * undertow_glow;
    color += rich_error * undertow_rim * 0.35;
    color += mix(rich_primary, rich_secondary, 0.35) * lower_line * 0.5;
    color += mix(rich_secondary, rich_surface, 0.2) * deep_band;
    color += mix(rich_secondary, rich_error, 0.3) * deep_glow;
    color += rich_error * deep_rim * 0.24;
    color += mix(rich_primary, rich_secondary, 0.5) * lower_razor_line * 0.45;

    // Keep the empty upper and lower thirds nearly black for contrast.
    color *= 0.78 + 0.06 * smoothstep(0.0, 1.0, local.x);
    color *= u_brightness * u_visibility;
    gl_FragColor = vec4(color, 1.0);
}
