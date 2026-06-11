#!/usr/bin/env python3
import argparse
import json
from reportlab.lib.pagesizes import A4, landscape
from reportlab.pdfgen import canvas
from reportlab.graphics.shapes import Drawing
from reportlab.graphics.barcode.qr import QrCodeWidget
from reportlab.graphics import renderPDF

# Convert millimeter to points
def mm_to_pt(mm):
    return mm * 72.0 / 25.4

def generate_pdf(rows, cols, size_mm, output_path):
    # A4 landscape size: 297mm x 210mm
    width_mm = 297
    height_mm = 210
    
    width_pt = mm_to_pt(width_mm)
    height_pt = mm_to_pt(height_mm)
    
    # Create canvas
    c = canvas.Canvas(output_path, pagesize=landscape(A4))
    
    # 1. Draw Chessboard
    grid_width_mm = cols * size_mm
    grid_height_mm = rows * size_mm
    
    # Position chessboard to the right side of the paper
    # Left column is reserved for QR code and ruler (approx 0 to 80 mm)
    # Right area goes from 80mm to 287mm (width 207mm)
    right_area_center_x = 80 + (297 - 80 - 10) / 2.0  # 188.5 mm
    right_area_center_y = height_mm / 2.0             # 105 mm
    
    chessboard_x_start = right_area_center_x - (grid_width_mm / 2.0)
    chessboard_y_start = right_area_center_y - (grid_height_mm / 2.0)
    
    c.saveState()
    c.setFillColorRGB(0, 0, 0)
    for r in range(rows):
        for col in range(cols):
            # Alternate black and white squares
            if (r + col) % 2 == 0:
                x = mm_to_pt(chessboard_x_start + col * size_mm)
                y = mm_to_pt(chessboard_y_start + r * size_mm)
                s = mm_to_pt(size_mm)
                c.rect(x, y, s, s, fill=1, stroke=0)
    c.restoreState()
    
    # 2. Draw QR Code
    # Placed in the left column (left side, middle)
    qr_size_mm = 45
    qr_x_mm = 20
    qr_y_mm = height_mm / 2.0 - qr_size_mm / 2.0  # Middle
    
    qr_data = {
        "rows": rows,
        "cols": cols,
        "size_mm": size_mm
    }
    qr_content = json.dumps(qr_data)
    
    qr_size_pt = mm_to_pt(qr_size_mm)
    qr_x_pt = mm_to_pt(qr_x_mm)
    qr_y_pt = mm_to_pt(qr_y_mm)
    
    d = Drawing(qr_size_pt, qr_size_pt)
    qr_widget = QrCodeWidget(qr_content)
    qr_widget.barWidth = qr_size_pt
    qr_widget.barHeight = qr_size_pt
    d.add(qr_widget)
    renderPDF.draw(d, c, qr_x_pt, qr_y_pt)
    
    # Add text label below QR code
    c.saveState()
    c.setFont("Helvetica-Bold", 8)
    c.drawCentredString(qr_x_pt + qr_size_pt/2.0, qr_y_pt - mm_to_pt(5), f"Chessboard: {rows}x{cols}")
    c.drawCentredString(qr_x_pt + qr_size_pt/2.0, qr_y_pt - mm_to_pt(10), f"Square Size: {size_mm} mm")
    c.restoreState()
    
    # 3. Draw Ruler (100mm / 10cm)
    # Placed at the bottom left
    ruler_x_start = 15
    ruler_y = 25
    ruler_length = 100
    
    c.saveState()
    c.setStrokeColorRGB(0, 0, 0)
    c.setLineWidth(1)
    
    # Main line
    c.line(mm_to_pt(ruler_x_start), mm_to_pt(ruler_y), mm_to_pt(ruler_x_start + ruler_length), mm_to_pt(ruler_y))
    
    # Draw ticks and labels
    c.setFont("Helvetica", 7)
    for i in range(ruler_length + 1):
        x = ruler_x_start + i
        x_pt = mm_to_pt(x)
        
        if i % 10 == 0:  # Major tick every 10mm (1cm)
            tick_h = 6
            c.line(x_pt, mm_to_pt(ruler_y), x_pt, mm_to_pt(ruler_y + tick_h))
            # Draw label
            cm_val = i // 10
            c.drawCentredString(x_pt, mm_to_pt(ruler_y + tick_h + 2), f"{cm_val}")
        elif i % 5 == 0:  # Medium tick every 5mm
            tick_h = 4
            c.line(x_pt, mm_to_pt(ruler_y), x_pt, mm_to_pt(ruler_y + tick_h))
        else:  # Minor tick every 1mm
            tick_h = 2
            c.line(x_pt, mm_to_pt(ruler_y), x_pt, mm_to_pt(ruler_y + tick_h))
            
    # Draw label "cm" at the end of the ruler
    c.drawString(mm_to_pt(ruler_x_start + ruler_length + 3), mm_to_pt(ruler_y), "cm")
    
    # Label describing the ruler
    c.drawString(mm_to_pt(ruler_x_start), mm_to_pt(ruler_y - 8), "Verification Scale (10 cm)")
    c.restoreState()
    
    # Save PDF
    c.showPage()
    c.save()
    print(f"Successfully generated calibration sheet PDF: {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate Chessboard Calibration PDF with QR Code and Ruler.")
    parser.add_argument("--rows", type=int, default=7, help="Number of rows of squares (default: 7)")
    parser.add_argument("--cols", type=int, default=8, help="Number of columns of squares (default: 8)")
    parser.add_argument("--size", type=float, default=20.0, help="Size of each square in mm (default: 20.0)")
    parser.add_argument("--output", type=str, default="calibration_sheet.pdf", help="Output PDF filename (default: calibration_sheet.pdf)")
    
    args = parser.parse_args()
    generate_pdf(args.rows, args.cols, args.size, args.output)
