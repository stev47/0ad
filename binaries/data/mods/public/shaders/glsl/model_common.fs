#version 120

uniform sampler2D baseTex;
uniform sampler2D losTex;
uniform sampler2D aoTex;
uniform sampler2D normTex;
uniform sampler2D specTex;

#if USE_SHADOW
  varying vec4 v_shadow;
  #if USE_SHADOW_SAMPLER
    uniform sampler2DShadow shadowTex;
    #if USE_SHADOW_PCF
      uniform vec4 shadowScale;
    #endif
  #else
    uniform sampler2D shadowTex;
  #endif
#endif

#if USE_OBJECTCOLOR
  uniform vec3 objectColor;
#else
#if USE_PLAYERCOLOR
  uniform vec3 playerColor;
#endif
#endif

uniform vec3 shadingColor;
uniform vec3 ambient;
uniform vec3 sunColor;
uniform vec3 sunDir;

varying vec4 v_lighting;
varying vec2 v_tex;
varying vec2 v_los;

#if USE_INSTANCING && USE_AO
  varying vec2 v_tex2;
#endif

#if USE_SPECULAR
  uniform float specularPower;
  uniform vec3 specularColor;
#endif

#if USE_SPECULAR || USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX_MAP || USE_AO
  uniform vec4 effectSettings;
#endif

#if USE_SPECULAR || USE_NORMAL_MAP || USE_SPECULAR_MAP || USE_PARALLAX_MAP
  varying vec4 v_normal;
  #if USE_INSTANCING && (USE_NORMAL_MAP || USE_PARALLAX_MAP)
    varying vec4 v_tangent;
    //varying vec3 v_bitangent;
  #endif
  #if USE_SPECULAR || USE_SPECULAR_MAP
    varying vec3 v_half;
  #endif
  #if USE_INSTANCING && USE_PARALLAX_MAP
    varying vec3 v_eyeVec;
  #endif
#endif


float get_shadow()
{
  #if USE_SHADOW && !DISABLE_RECEIVE_SHADOWS
    #if USE_SHADOW_SAMPLER
      #if USE_SHADOW_PCF
        vec2 offset = fract(v_shadow.xy - 0.5);
        vec4 size = vec4(offset + 1.0, 2.0 - offset);
        vec4 weight = (vec4(2.0 - 1.0 / size.xy, 1.0 / size.zw - 1.0) + (v_shadow.xy - offset).xyxy) * shadowScale.zwzw;
        return (1.0/9.0)*dot(size.zxzx*size.wwyy,
          vec4(shadow2D(shadowTex, vec3(weight.zw, v_shadow.z)).r,
               shadow2D(shadowTex, vec3(weight.xw, v_shadow.z)).r,
               shadow2D(shadowTex, vec3(weight.zy, v_shadow.z)).r,
               shadow2D(shadowTex, vec3(weight.xy, v_shadow.z)).r));
      #else
        return shadow2D(shadowTex, v_shadow.xyz).r;
      #endif
    #else
      if (v_shadow.z >= 1.0)
        return 1.0;
      return (v_shadow.z <= texture2D(shadowTex, v_shadow.xy).x ? 1.0 : 0.0);
    #endif
  #else
    return 1.0;
  #endif
}

void main()
{
  vec2 coord = v_tex;

  #if USE_PARALLAX_MAP || USE_NORMAL_MAP
    vec3 bitangent = vec3(v_normal.w, v_tangent.w, v_lighting.w);
    mat3 tbn = mat3(v_tangent.xyz, bitangent, v_normal.xyz);
  #endif

  #if USE_PARALLAX_MAP
  {
    float h = texture2D(normTex, coord).a;

    vec3 eyeDir = normalize(v_eyeVec * tbn);
    float dist = length(v_eyeVec);

    float s;
    vec2 move;
    float height = 1.0;
    float scale = effectSettings.z;
      
    scale = (scale * (PARALLAX_DIST_MAX - dist)) / PARALLAX_DIST_MAX;

    float iter = 8.0;
    #if USE_HQ_PARALLAX
      iter = 16.0;
    #endif

    s = 1.0 / iter;
    move = vec2(-eyeDir.x, eyeDir.y) * scale / (eyeDir.z * iter);
    vec2 nil = vec2(0.0);

    #define PARALLAX_ITER {\
      height -= s;\
      vec2 temp = (h < height) ? move : nil;\
      coord += temp;\
      h = texture2D(normTex, coord).a;\
    }

    // 8 iterations of parallax 
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER
    PARALLAX_ITER

    // an additional 8 iterations for the HQ parallax
    #if USE_HQ_PARALLAX
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
      PARALLAX_ITER
    #endif      
  }
  #endif

  vec4 tex = texture2D(baseTex, coord);

  // Alpha-test as early as possible
  #ifdef REQUIRE_ALPHA_GEQUAL
    if (tex.a < REQUIRE_ALPHA_GEQUAL)
      discard;
  #endif

  #if USE_TRANSPARENT
    gl_FragColor.a = tex.a;
  #else
    gl_FragColor.a = 1.0;
  #endif
  
  vec3 texdiffuse = tex.rgb;

  // Apply-coloring based on texture alpha
  #if USE_OBJECTCOLOR
    texdiffuse *= mix(objectColor, vec3(1.0, 1.0, 1.0), tex.a);
  #else
  #if USE_PLAYERCOLOR
    texdiffuse *= mix(playerColor, vec3(1.0, 1.0, 1.0), tex.a);
  #endif
  #endif

  #if USE_SPECULAR || USE_SPECULAR_MAP || USE_NORMAL_MAP
    vec3 normal = v_normal.xyz;
  #endif

  #if USE_INSTANCING && USE_NORMAL_MAP
    vec3 ntex = texture2D(normTex, coord).rgb * 2.0 - 1.0;
    ntex.y = -ntex.y;
    normal = normalize(tbn * ntex);
    vec3 bumplight = max(dot(-sunDir, normal), 0.0) * sunColor;
    vec3 sundiffuse = (bumplight - v_lighting.rgb) * effectSettings.x + v_lighting.rgb;
  #else
    vec3 sundiffuse = v_lighting.rgb;
  #endif

  vec4 specular = vec4(0.0);
  #if USE_SPECULAR || USE_SPECULAR_MAP
    vec3 specCol;
    float specPow;
    #if USE_SPECULAR_MAP
      vec4 s = texture2D(specTex, coord);
      specCol = s.rgb;
      specular.a = s.a;
      specPow = effectSettings.y;
    #else
      specCol = specularColor;
      specPow = specularPower;
    #endif
    specular.rgb = sunColor * specCol * pow(max(0.0, dot(normalize(normal), v_half)), specPow);
  #endif

  vec3 color = (texdiffuse * sundiffuse + specular.rgb) * get_shadow();
  vec3 ambColor = texdiffuse * ambient;

  #if USE_INSTANCING && USE_AO
    vec3 ao = texture2D(aoTex, v_tex2).rrr;
    ao = mix(vec3(1.0), ao * 2.0, effectSettings.w);
    ambColor *= ao;
  #endif

  color += ambColor;

  #if USE_SPECULAR_MAP && USE_SELF_LIGHT
    color = mix(texdiffuse, color, specular.a);
  #endif

  #if !IGNORE_LOS
    float los = texture2D(losTex, v_los).a;
    color *= los;
  #endif

  color *= shadingColor;

  gl_FragColor.rgb = color;
}
