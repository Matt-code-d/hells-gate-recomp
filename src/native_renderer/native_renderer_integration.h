








#pragma once

#include <memory>

namespace dante {



class NativeRendererIntegration {
 public:
  NativeRendererIntegration();
  ~NativeRendererIntegration();

  
  
  
  
  bool initialize(void* hwnd, uint32_t width, uint32_t height);

  
  
  
  void onFrame();

  
  void onResize(uint32_t width, uint32_t height);

  
  void shutdown();

  
  bool isInitialized() const;

  
  bool isActive() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  
