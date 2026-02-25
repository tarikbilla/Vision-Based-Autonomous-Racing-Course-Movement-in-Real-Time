#!/usr/bin/env python3
"""
Script to fix OpenCV 4.10.0 compatibility issues in the RC car control project.
Run this script from the project root directory.
"""

import os
import re
import sys

def fix_object_tracker():
    """Fix object_tracker.cpp for OpenCV 4.10.0 compatibility"""
    filepath = 'src/object_tracker.cpp'
    
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found!")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Fix 1: Replace MOSSE tracker
    old_mosse = r'case TrackerType::MOSSE:\s+return cv::TrackerMOSSE::create\(\);'
    new_mosse = '''case TrackerType::MOSSE:
            // MOSSE tracker was removed in OpenCV 4.5.1+, fallback to KCF
            std::cerr << "Warning: MOSSE tracker not available in OpenCV 4.10.0, using KCF instead" << std::endl;
            return cv::TrackerKCF::create();'''
    
    content = re.sub(old_mosse, new_mosse, content)
    
    # Fix 2: Replace init() call
    old_init = r'if \(!tracker_->init\(frame, bbox\)\) \{\s+std::cerr << "Error: Failed to initialize tracker" << std::endl;\s+initialized_ = false;\s+return false;\s+\}'
    new_init = '''// In OpenCV 4.x, init() returns void, not bool
    try {
        tracker_->init(frame, bbox);
    } catch (const cv::Exception& e) {
        std::cerr << "Error: Failed to initialize tracker: " << e.what() << std::endl;
        initialized_ = false;
        return false;
    }'''
    
    content = re.sub(old_init, new_init, content, flags=re.DOTALL)
    
    # Fix 3: Replace update() call
    old_update = r'bool ok = tracker_->update\(frame, bbox_\);'
    new_update = '''// Update tracker - need cv::Rect (int) not cv::Rect2d (double)
    cv::Rect bbox_int;
    bool ok = tracker_->update(frame, bbox_int);
    
    // Convert back to Rect2d for storage
    bbox_ = cv::Rect2d(bbox_int);'''
    
    content = re.sub(old_update, new_update, content)
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Fixed {filepath}")
    return True

def fix_boundary_detection():
    """Fix boundary_detection.cpp - remove unused variable"""
    filepath = 'src/boundary_detection.cpp'
    
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found!")
        return False
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
    
    new_lines = []
    skip_next_min_index = False
    
    for i, line in enumerate(lines):
        # Remove the min_index declaration
        if 'int min_index = 0;' in line:
            continue
        
        # Remove min_index assignment
        if 'min_index = static_cast<int>(i);' in line:
            continue
        
        # Keep the distance check but remove min_index assignment
        if 'if (rays_[i].distance < min_distance)' in line:
            new_lines.append(line)
            # Check if next line has min_index assignment
            if i + 1 < len(lines) and 'min_index' in lines[i + 1]:
                skip_next_min_index = True
            continue
        
        if skip_next_min_index and 'min_index' in line:
            skip_next_min_index = False
            continue
        
        new_lines.append(line)
    
    with open(filepath, 'w') as f:
        f.writelines(new_lines)
    
    print(f"✅ Fixed {filepath}")
    return True

def fix_camera_capture():
    """Fix camera_capture.cpp - fix initialization order"""
    filepath = 'src/camera_capture.cpp'
    
    if not os.path.exists(filepath):
        print(f"Error: {filepath} not found!")
        return False
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Fix first constructor
    old_ctor1 = r'CameraCapture::CameraCapture\(\)\s+:\s+camera_index_\(0\),\s+target_width_\(1920\),\s+target_height_\(1080\),\s+target_fps_\(30\),\s+running_\(false\),\s+paused_\(false\)'
    new_ctor1 = 'CameraCapture::CameraCapture()\n    : running_(false), paused_(false),\n      camera_index_(0), target_width_(1920), target_height_(1080), target_fps_(30)'
    
    content = re.sub(old_ctor1, new_ctor1, content)
    
    # Fix second constructor
    old_ctor2 = r'CameraCapture::CameraCapture\(int camera_index\)\s+:\s+camera_index_\(camera_index\),\s+target_width_\(1920\),\s+target_height_\(1080\),\s+target_fps_\(30\),\s+running_\(false\),\s+paused_\(false\)'
    new_ctor2 = 'CameraCapture::CameraCapture(int camera_index)\n    : running_(false), paused_(false),\n      camera_index_(camera_index), target_width_(1920), target_height_(1080), target_fps_(30)'
    
    content = re.sub(old_ctor2, new_ctor2, content)
    
    with open(filepath, 'w') as f:
        f.write(content)
    
    print(f"✅ Fixed {filepath}")
    return True

def main():
    print("=" * 60)
    print("Fixing OpenCV 4.10.0 Compatibility Issues")
    print("=" * 60)
    print()
    
    # Check if we're in the right directory
    if not os.path.exists('src') or not os.path.exists('include'):
        print("Error: Please run this script from the project root directory")
        print("Expected structure: src/, include/, CMakeLists.txt")
        sys.exit(1)
    
    success = True
    success &= fix_object_tracker()
    success &= fix_boundary_detection()
    success &= fix_camera_capture()
    
    print()
    if success:
        print("=" * 60)
        print("✅ All fixes applied successfully!")
        print("=" * 60)
        print()
        print("Next steps:")
        print("  cd build")
        print("  rm -rf *")
        print("  cmake -DCMAKE_BUILD_TYPE=Release ..")
        print("  make -j4")
    else:
        print("=" * 60)
        print("❌ Some fixes failed. Please check the errors above.")
        print("=" * 60)
        sys.exit(1)

if __name__ == '__main__':
    main()
