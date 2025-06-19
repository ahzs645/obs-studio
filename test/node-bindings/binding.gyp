{
  "targets": [
    {
      "target_name": "obs_screen_capture",
      "sources": [
        "src/obs_screen_capture.cpp"
      ],
      "conditions": [
        ["OS=='mac'", {
          "sources": ["src/permission_manager.mm"],
          "libraries": [
            "-framework Cocoa",
            "-framework CoreGraphics"
          ]
        }],
        ["OS=='linux'", {
          "libraries": ["-lX11", "-lXrandr"]
        }],
        ["OS=='win'", {
          "libraries": ["-lgdi32"]
        }]
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../..",
        "../../libobs",
        "/root/.nvm/versions/node/v20.19.2/include/node"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "libraries": [
        "-lobs",
        "-lpthread"
      ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc": [ "-std=c++17" ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ]
    }
  ]
}
