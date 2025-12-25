#!/usr/bin/env python3
"""
Simplified and correct involute gear generator with symmetric teeth.
This version uses a clear, step-by-step approach to ensure symmetry.
"""

import numpy as np
import matplotlib.pyplot as plt
import math

class InvoluteGear:
    def __init__(self, num_teeth=12, module=2.1336, pressure_angle_deg=20.0):
        # Basic parameters
        self.num_teeth = num_teeth
        self.module = module
        self.pressure_angle = math.radians(pressure_angle_deg)
        
        # Derived parameters
        self.pitch_diameter = module * num_teeth
        self.pitch_radius = self.pitch_diameter / 2
        self.base_radius = self.pitch_radius * math.cos(self.pressure_angle)
        self.addendum = 1.0 * module
        self.dedendum = 1.25 * module
        self.tip_radius = self.pitch_radius + self.addendum
        self.root_radius = self.pitch_radius - self.dedendum
        self.tooth_thickness = math.pi * module / 2  # At pitch circle
        self.tooth_angle = 2 * math.pi / num_teeth
    
    def involute_polar(self, base_radius, angles):
        """
        Generate involute curve in polar coordinates.
        Returns (radius, theta) for each unwrap angle.
        """
        # For an involute:
        # radius = base_radius * sqrt(1 + angle^2)
        # theta = angle - atan(angle)
        
        radius = base_radius * np.sqrt(1 + angles**2)
        theta = angles - np.arctan(angles)
        return radius, theta
    
    def generate_single_tooth(self):
        """
        Generate a single symmetric tooth centered on the positive y-axis.
        """
        # Calculate involute angles
        max_angle = math.sqrt((self.tip_radius / self.base_radius)**2 - 1)
        
        # Generate involute curve points
        num_points = 30
        angles = np.linspace(0, max_angle, num_points)
        r, theta = self.involute_polar(self.base_radius, angles)
        
        # Calculate the angle offset to get correct tooth thickness at pitch circle
        # The involute angle at pitch radius
        pitch_angle = math.sqrt((self.pitch_radius / self.base_radius)**2 - 1)
        pitch_involute_angle = pitch_angle - math.atan(pitch_angle)
        
        # Half tooth thickness angle at pitch radius
        half_tooth_angle = self.tooth_thickness / (2 * self.pitch_radius)
        
        # Offset to position the involute correctly
        offset_angle = half_tooth_angle - pitch_involute_angle
        
        # Create right side of tooth (positive angles)
        theta_right = theta + offset_angle
        x_right = r * np.cos(theta_right)
        y_right = r * np.sin(theta_right)
        
        # Create left side of tooth (mirror of right side)
        theta_left = -theta - offset_angle
        x_left = r * np.cos(theta_left)
        y_left = r * np.sin(theta_left)
        
        # Combine the sides
        # Left side (reversed) + root + right side
        x_left_rev = x_left[::-1]
        y_left_rev = y_left[::-1]
        
        # Add root circle connection if needed
        if self.root_radius < self.base_radius:
            # Connect with root circle arc
            angle_left = math.atan2(y_left_rev[-1], x_left_rev[-1])
            angle_right = math.atan2(y_right[0], x_right[0])
            
            if angle_left > angle_right:
                angle_left -= 2 * math.pi
            
            root_angles = np.linspace(angle_left, angle_right, 10)
            x_root = self.root_radius * np.cos(root_angles)
            y_root = self.root_radius * np.sin(root_angles)
            
            # Combine all parts
            x_tooth = np.concatenate([x_left_rev, x_root, x_right])
            y_tooth = np.concatenate([y_left_rev, y_root, y_right])
        else:
            # Direct connection at base circle
            x_tooth = np.concatenate([x_left_rev, x_right])
            y_tooth = np.concatenate([y_left_rev, y_right])
        
        # Rotate tooth to be centered on positive y-axis (pointing up)
        rotation = math.pi / 2
        x_rotated = x_tooth * math.cos(rotation) - y_tooth * math.sin(rotation)
        y_rotated = x_tooth * math.sin(rotation) + y_tooth * math.cos(rotation)
        
        return x_rotated, y_rotated
    
    def generate_all_teeth(self):
        """Generate all teeth for the complete gear."""
        x_base, y_base = self.generate_single_tooth()
        
        teeth = []
        for i in range(self.num_teeth):
            angle = i * self.tooth_angle
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)
            
            # Rotate the base tooth
            x_rotated = x_base * cos_a - y_base * sin_a
            y_rotated = x_base * sin_a + y_base * cos_a
            
            teeth.append((x_rotated, y_rotated))
        
        return teeth
    
    def verify_symmetry(self, x_tooth, y_tooth):
        """Check if a tooth is symmetric about its centerline."""
        n = len(x_tooth)
        errors = []
        
        # The tooth should be symmetric about x=0 (vertical axis)
        for i in range(n // 3):
            # Find corresponding point on opposite side
            j = n - i - 1
            
            if j < n:
                # Points should have x-coordinates that are negatives of each other
                # and same y-coordinates
                x_error = abs(x_tooth[i] + x_tooth[j])
                y_error = abs(y_tooth[i] - y_tooth[j])
                total_error = math.sqrt(x_error**2 + y_error**2)
                errors.append(total_error)
        
        return errors
    
    def plot(self, save_filename='symmetric_involute_gear.png'):
        """Create comprehensive plots of the gear."""
        teeth = self.generate_all_teeth()
        x_single, y_single = self.generate_single_tooth()
        
        fig = plt.figure(figsize=(16, 12))
        
        # Main gear plot
        ax1 = plt.subplot(2, 2, (1, 3))
        
        # Plot all teeth
        for i, (x, y) in enumerate(teeth):
            if i == 0:
                ax1.plot(x, y, 'r-', linewidth=2.5, label='First tooth', zorder=3)
            else:
                ax1.plot(x, y, 'b-', linewidth=2)
        
        # Plot reference circles
        theta = np.linspace(0, 2*math.pi, 200)
        ax1.plot(self.pitch_radius * np.cos(theta),
                self.pitch_radius * np.sin(theta),
                'g--', linewidth=1, alpha=0.6, label='Pitch circle')
        ax1.plot(self.base_radius * np.cos(theta),
                self.base_radius * np.sin(theta),
                'r--', linewidth=1, alpha=0.6, label='Base circle')
        ax1.plot(self.tip_radius * np.cos(theta),
                self.tip_radius * np.sin(theta),
                'm--', linewidth=1, alpha=0.6, label='Tip circle')
        ax1.plot(self.root_radius * np.cos(theta),
                self.root_radius * np.sin(theta),
                'c--', linewidth=1, alpha=0.6, label='Root circle')
        
        ax1.plot(0, 0, 'k+', markersize=12)
        ax1.set_aspect('equal')
        ax1.grid(True, alpha=0.3)
        ax1.legend(loc='upper right', fontsize=9)
        ax1.set_title(f'Involute Gear - {self.num_teeth} teeth, Module = {self.module:.4f} mm',
                     fontsize=12, fontweight='bold')
        ax1.set_xlabel('X (mm)')
        ax1.set_ylabel('Y (mm)')
        
        # Single tooth detail
        ax2 = plt.subplot(2, 2, 2)
        ax2.plot(x_single, y_single, 'b-', linewidth=3, label='Tooth profile')
        ax2.axvline(x=0, color='k', linestyle='--', alpha=0.5, linewidth=1,
                   label='Symmetry axis')
        
        # Mark symmetric points
        n = len(x_single)
        for i in range(0, n, n//10):
            ax2.plot(x_single[i], y_single[i], 'ro', markersize=4)
            # Find symmetric point
            for j in range(n):
                if abs(y_single[j] - y_single[i]) < 0.1 and abs(x_single[j] + x_single[i]) < 0.5:
                    ax2.plot([x_single[i], x_single[j]], 
                           [y_single[i], y_single[j]], 
                           'c-', alpha=0.3, linewidth=1)
                    break
        
        ax2.set_aspect('equal')
        ax2.grid(True, alpha=0.3)
        ax2.legend(loc='upper right')
        ax2.set_title('Single Tooth - Symmetry Check')
        ax2.set_xlabel('X (mm)')
        ax2.set_ylabel('Y (mm)')
        
        # Symmetry error plot
        ax3 = plt.subplot(2, 2, 4)
        errors = self.verify_symmetry(x_single, y_single)
        if errors:
            ax3.plot(errors, 'b-', linewidth=2, label='Symmetry error')
            ax3.axhline(y=0.001, color='g', linestyle=':', alpha=0.7,
                       label='0.001 mm tolerance')
            ax3.axhline(y=0.01, color='y', linestyle=':', alpha=0.7,
                       label='0.01 mm tolerance')
            ax3.set_xlabel('Point pair index')
            ax3.set_ylabel('Symmetry error (mm)')
            ax3.set_title('Symmetry Error Analysis')
            ax3.legend()
            ax3.grid(True, alpha=0.3)
            ax3.set_yscale('log')
            
            max_error = max(errors) if errors else 0
            avg_error = sum(errors) / len(errors) if errors else 0
            ax3.text(0.02, 0.98, 
                    f'Max error: {max_error:.6f} mm\nAvg error: {avg_error:.6f} mm',
                    transform=ax3.transAxes, verticalalignment='top',
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.7))
        
        plt.suptitle('Symmetric Involute Gear Profile Analysis', 
                    fontsize=14, fontweight='bold')
        plt.tight_layout()
        plt.savefig(save_filename, dpi=150, bbox_inches='tight')
        plt.show()
        
        return fig

# Main execution
if __name__ == "__main__":
    print("="*70)
    print("SYMMETRIC INVOLUTE GEAR GENERATOR - SIMPLIFIED VERSION")
    print("="*70)
    
    # Create gear matching your specifications
    gear = InvoluteGear(num_teeth=42, module=2.1336, pressure_angle_deg=20.0)
    
    print(f"\nGear Parameters:")
    print(f"  Number of teeth: {gear.num_teeth}")
    print(f"  Module: {gear.module:.4f} mm")
    print(f"  Pressure angle: {20.0}°")
    print(f"  Pitch diameter: {gear.pitch_diameter:.3f} mm")
    print(f"  Base diameter: {2*gear.base_radius:.3f} mm")
    print(f"  Tip diameter: {2*gear.tip_radius:.3f} mm")
    print(f"  Root diameter: {2*gear.root_radius:.3f} mm")
    
    # Generate and verify single tooth
    x_tooth, y_tooth = gear.generate_single_tooth()
    errors = gear.verify_symmetry(x_tooth, y_tooth)
    
    print(f"\nSymmetry Analysis:")
    if errors:
        max_error = max(errors)
        avg_error = sum(errors) / len(errors)
        print(f"  Maximum error: {max_error:.6f} mm")
        print(f"  Average error: {avg_error:.6f} mm")
        
        if max_error < 0.001:
            print("  ✓ EXCELLENT: Tooth is symmetric within 0.001 mm")
        elif max_error < 0.01:
            print("  ✓ GOOD: Tooth is symmetric within 0.01 mm")
        elif max_error < 0.1:
            print("  ⚠ ACCEPTABLE: Tooth is symmetric within 0.1 mm")
        else:
            print("  ✗ POOR: Symmetry error exceeds 0.1 mm")
    
    # Create visualization
    print("\nGenerating visualization...")
    gear.plot('involute_gear_symmetric_final.png')
    
    print("\n" + "="*70)
    print("Complete! Check 'involute_gear_symmetric_final.png'")
    print("The gear teeth should now be perfectly symmetric.")
