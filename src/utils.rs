// Import external modules or crates needed in utils.rs
use crate::dataset::Dataset;
// Import external modules or crates needed in utils.rs
use crate::app::{FontSize, Subplot, SubplotLayout};
// Import external modules or crates needed in utils.rs
use std::fs::File;
// Import external modules or crates needed in utils.rs
use std::io::{BufRead, BufReader};
// Import external modules or crates needed in utils.rs
use std::path::PathBuf;

/// Data structure used in utils.rs module
pub struct AxisConfig {
   pub x_min: Option<f64>,
   pub x_max: Option<f64>,
   pub y_min: Option<f64>,
   pub y_max: Option<f64>,
   pub x_padding_percent: f64,
   pub y_padding_percent: f64,
   pub custom_x_ticks: Option<Vec<f64>>,
   pub custom_y_ticks: Option<Vec<f64>>,
}

// Helper function to parse custom ticks from comma-separated string
/// Function: explain its purpose and key arguments
pub fn parse_custom_ticks(ticks_str: &str) -> Vec<f64> {
    ticks_str
        .split(',')
        .filter_map(|s| s.trim().parse::<f64>().ok())
        .collect()
}

// Helper function to compute rolling average
/// Function: explain its purpose and key arguments
pub fn compute_rolling_average(points: &[[f64; 2]], window_size: usize) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
    if window_size == 0 {
        return Err("Window size must be greater than 0".into());
    }
    
    if points.len() < window_size {
        return Err("Window size cannot be larger than dataset size".into());
    }
    
// Variable declaration
    let mut result = Vec::new();
    
    // Compute rolling average
    for i in 0..=(points.len() - window_size) {
// Variable declaration
        let window_slice = &points[i..i + window_size];
        
        // Calculate average X and Y for this window
// Variable declaration
        let avg_x: f64 = window_slice.iter().map(|p| p[0]).sum::<f64>() / window_size as f64;
// Variable declaration
        let avg_y: f64 = window_slice.iter().map(|p| p[1]).sum::<f64>() / window_size as f64;
        
        result.push([avg_x, avg_y]);
    }
    
    Ok(result)
}

// Helper function to get data bounds
/// Function: explain its purpose and key arguments
pub fn get_data_bounds(datasets: &[Dataset]) -> Option<(f64, f64, f64, f64)> {
    if datasets.is_empty() {
        return None;
    }
    
// Variable declaration
    let mut min_x = f64::INFINITY;
// Variable declaration
    let mut max_x = f64::NEG_INFINITY;
// Variable declaration
    let mut min_y = f64::INFINITY;
// Variable declaration
    let mut max_y = f64::NEG_INFINITY;
    
    for dataset in datasets {
        for point in &dataset.points {
            min_x = min_x.min(point[0]);
            max_x = max_x.max(point[0]);
            min_y = min_y.min(point[1]);
            max_y = max_y.max(point[1]);
        }
    }
    
    Some((min_x, max_x, min_y, max_y))
}

// New function to export subplots as PNG
/// Function: explain its purpose and key arguments
pub fn export_subplots_as_png(
    subplots: &[Subplot],
    layout: &SubplotLayout,
    dark_mode: bool,
    font_size: &FontSize,
) -> Result<(), Box<dyn std::error::Error>> {
    if subplots.is_empty() {
        return Err("No subplots to export".into());
    }

    if let Some(path) = rfd::FileDialog::new()
        .add_filter("PNG Image", &["png"])
        .set_file_name("subplots.png")
        .save_file()
    {
// Variable declaration
        let (rows, cols) = layout.dimensions();
        
        // Calculate image dimensions based on subplot layout
// Variable declaration
        let subplot_width = 600u32;
// Variable declaration
        let subplot_height = 400u32;
// Variable declaration
        let _margin = 80u32;
// Variable declaration
        let spacing = 40u32;
        
// Variable declaration
        let total_width = cols as u32 * subplot_width + (cols as u32 + 1) * spacing;
// Variable declaration
        let total_height = rows as u32 * subplot_height + (rows as u32 + 1) * spacing + 60; // Extra space for titles

// Variable declaration
        let (bg_color, grid_color, axis_color, text_color) = if dark_mode {
            (
                image::Rgb([27, 27, 27]),
                image::Rgb([60, 60, 60]),
                image::Rgb([180, 180, 180]),
                image::Rgb([255, 255, 255]),
            )
        } else {
            (
                image::Rgb([248, 248, 248]),
                image::Rgb([200, 200, 200]),
                image::Rgb([100, 100, 100]),
                image::Rgb([0, 0, 0]),
            )
        };

// Variable declaration
        let mut img_buffer = image::RgbImage::new(total_width, total_height);
        for pixel in img_buffer.pixels_mut() {
            *pixel = bg_color;
        }

        // Draw each subplot
        for (subplot_idx, subplot) in subplots.iter().enumerate() {
            if subplot_idx >= rows * cols {
                break;
            }
            
// Variable declaration
            let row = subplot_idx / cols;
// Variable declaration
            let col = subplot_idx % cols;
            
// Variable declaration
            let subplot_x = spacing + col as u32 * (subplot_width + spacing);
// Variable declaration
            let subplot_y = spacing + row as u32 * (subplot_height + spacing);
            
            render_subplot_to_image(
                &mut img_buffer,
                subplot,
                subplot_x,
                subplot_y,
                subplot_width,
                subplot_height,
                bg_color,
                grid_color,
                axis_color,
                text_color,
                font_size,
                subplot_idx + 1,
            )?;
        }

        img_buffer.save(&path)?;
        println!("Subplots exported as: {}", path.display());
    }
    Ok(())
}

/// Function: explain its purpose and key arguments
fn render_subplot_to_image(
    img: &mut image::RgbImage,
    subplot: &Subplot,
    x_offset: u32,
    y_offset: u32,
    width: u32,
    height: u32,
    _bg_color: image::Rgb<u8>,
    grid_color: image::Rgb<u8>,
    axis_color: image::Rgb<u8>,
    text_color: image::Rgb<u8>,
    font_size: &FontSize,
    subplot_number: usize,
) -> Result<(), Box<dyn std::error::Error>> {
    if subplot.datasets.is_empty() {
        // Draw empty subplot with title
        draw_subplot_title(img, x_offset, y_offset, width, &subplot.config.title, subplot_number, text_color, font_size);
        draw_empty_subplot_frame(img, x_offset, y_offset + 30, width, height - 30, axis_color);
        return Ok(());
    }

    // Calculate bounds
// Variable declaration
    let (min_x, max_x, min_y, max_y) = if subplot.config.use_custom_bounds {
// Variable declaration
        let config = AxisConfig {
            x_min: subplot.config.custom_x_min.parse().ok(),
            x_max: subplot.config.custom_x_max.parse().ok(),
            y_min: subplot.config.custom_y_min.parse().ok(),
            y_max: subplot.config.custom_y_max.parse().ok(),
            x_padding_percent: subplot.config.x_padding_percent / 100.0,
            y_padding_percent: subplot.config.y_padding_percent / 100.0,
            custom_x_ticks: if subplot.config.use_custom_x_ticks {
                Some(parse_custom_ticks(&subplot.config.custom_x_ticks))
            } else {
                None
            },
            custom_y_ticks: if subplot.config.use_custom_y_ticks {
                Some(parse_custom_ticks(&subplot.config.custom_y_ticks))
            } else {
                None
            },
        };
        calculate_custom_bounds(&subplot.datasets, &config)?
    } else {
        calculate_auto_bounds(&subplot.datasets)
    };

    // Draw subplot title
    draw_subplot_title(img, x_offset, y_offset, width, &subplot.config.title, subplot_number, text_color, font_size);

// Variable declaration
    let plot_y_offset = y_offset + 30; // Space for title
// Variable declaration
    let plot_height = height - 30;

// Variable declaration
    let margin_left = 60u32;
// Variable declaration
    let margin_right = 20u32;
// Variable declaration
    let margin_top = 20u32;
// Variable declaration
    let margin_bottom = 40u32;
// Variable declaration
    let plot_width = width - margin_left - margin_right;
// Variable declaration
    let effective_plot_height = plot_height - margin_top - margin_bottom;

    // Draw grid if requested
    if subplot.config.show_grid {
// Variable declaration
        let num_v_lines = 6;
        for i in 1..num_v_lines {
// Variable declaration
            let x = x_offset + margin_left + (i * plot_width / num_v_lines);
            for y in (plot_y_offset + margin_top)..(plot_y_offset + plot_height - margin_bottom) {
                if y % 3 == 0 {
                    img.put_pixel(x, y, grid_color);
                }
            }
        }
// Variable declaration
        let num_h_lines = 4;
        for i in 1..num_h_lines {
// Variable declaration
            let y = plot_y_offset + margin_top + (i * effective_plot_height / num_h_lines);
            for x in (x_offset + margin_left)..(x_offset + width - margin_right) {
                if x % 3 == 0 {
                    img.put_pixel(x, y, grid_color);
                }
            }
        }
    }

    // Draw axes
// Variable declaration
    let x_axis_y = plot_y_offset + plot_height - margin_bottom;
// Variable declaration
    let y_axis_x = x_offset + margin_left;
    for x in (x_offset + margin_left)..(x_offset + width - margin_right) {
        img.put_pixel(x, x_axis_y, axis_color);
    }
    for y in (plot_y_offset + margin_top)..(plot_y_offset + plot_height - margin_bottom) {
        img.put_pixel(y_axis_x, y, axis_color);
    }

    // Draw axis labels
    draw_subplot_axis_labels(
        img,
        min_x,
        max_x,
        min_y,
        max_y,
        x_offset + margin_left,
        margin_bottom,
        plot_width,
        effective_plot_height,
        x_offset + width,
        plot_y_offset + plot_height,
        text_color,
        font_size,
    );

    // Draw datasets
    for dataset in &subplot.datasets {
// Variable declaration
        let rgb_color = image::Rgb(dataset.color);
        
        for window in dataset.points.windows(2) {
// Variable declaration
            let p1 = &window[0];
// Variable declaration
            let p2 = &window[1];
// Variable declaration
            let x1 = x_offset + margin_left + ((p1[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
// Variable declaration
            let y1 = plot_y_offset + plot_height - margin_bottom
                - ((p1[1] - min_y) / (max_y - min_y) * effective_plot_height as f64) as u32;
// Variable declaration
            let x2 = x_offset + margin_left + ((p2[0] - min_x) / (max_x - min_x) * plot_width as f64) as u32;
// Variable declaration
            let y2 = plot_y_offset + plot_height - margin_bottom
                - ((p2[1] - min_y) / (max_y - min_y) * effective_plot_height as f64) as u32;
            draw_thick_line(img, x1, y1, x2, y2, rgb_color, 2);
        }
    }

    // Draw legend if requested
    if subplot.config.show_legend && !subplot.datasets.is_empty() {
        draw_subplot_legend(
            img,
            &subplot.datasets,
            &subplot.config.legend_title,
            x_offset + width - 150,
            plot_y_offset + margin_top + 10,
            text_color,
            font_size,
        );
    }

    Ok(())
}

/// Function: explain its purpose and key arguments
fn draw_subplot_title(
    img: &mut image::RgbImage,
    x_offset: u32,
    y_offset: u32,
    width: u32,
    title: &str,
    subplot_number: usize,
    color: image::Rgb<u8>,
    font_size: &FontSize,
) {
// Variable declaration
    let display_title = if title.is_empty() {
        format!("Subplot {}", subplot_number)
    } else {
        format!("Subplot {}: {}", subplot_number, title)
    };
    
// Variable declaration
    let font_scale = font_size.to_scale() * 1.2; // Slightly larger for titles
// Variable declaration
    let char_width = (6.0 * font_scale) as u32;
// Variable declaration
    let title_width = display_title.len() as u32 * char_width;
// Variable declaration
    let title_x = x_offset + (width - title_width) / 2; // Center the title
    
    draw_text_scaled(img, title_x, y_offset + 5, &display_title, color, font_scale);
}

/// Function: explain its purpose and key arguments
fn draw_empty_subplot_frame(
    img: &mut image::RgbImage,
    x_offset: u32,
    y_offset: u32,
    width: u32,
    height: u32,
    color: image::Rgb<u8>,
) {
    // Draw border
    for x in x_offset..(x_offset + width) {
        img.put_pixel(x, y_offset, color);
        img.put_pixel(x, y_offset + height - 1, color);
    }
    for y in y_offset..(y_offset + height) {
        img.put_pixel(x_offset, y, color);
        img.put_pixel(x_offset + width - 1, y, color);
    }
}

/// Function: explain its purpose and key arguments
fn draw_subplot_axis_labels(
    img: &mut image::RgbImage,
    min_x: f64,
    max_x: f64,
    min_y: f64,
    max_y: f64,
    margin_left: u32,
    margin_bottom: u32,
    plot_width: u32,
    plot_height: u32,
    _total_width: u32,
    total_height: u32,
    color: image::Rgb<u8>,
    font_size: &FontSize,
) {
// Variable declaration
    let font_scale = font_size.to_scale();
    
    // X-axis labels (fewer ticks for subplots)
    for i in 0..=3 {
// Variable declaration
        let tick_value = min_x + (max_x - min_x) * (i as f64 / 3.0);
// Variable declaration
        let x_pos = margin_left + ((tick_value - min_x) / (max_x - min_x) * plot_width as f64) as u32;
// Variable declaration
        let tick_y = total_height - margin_bottom;
        
        // Draw tick mark
        for dy in 0..5 {
            if tick_y + dy < img.height() {
                img.put_pixel(x_pos, tick_y + dy, color);
            }
        }
        
        // Draw label
// Variable declaration
        let char_width = (6.0 * font_scale) as u32;
// Variable declaration
        let text = format_number(tick_value);
// Variable declaration
        let text_width = text.len() as u32 * char_width;
// Variable declaration
        let label_x = if x_pos >= text_width / 2 {
            x_pos - text_width / 2
        } else {
            0
        };
        
        draw_number_pixels_scaled(img, label_x, tick_y + 8, tick_value, color, font_scale);
    }

    // Y-axis labels
    for i in 0..=3 {
// Variable declaration
        let tick_value = min_y + (max_y - min_y) * (i as f64 / 3.0);
// Variable declaration
        let y_pos = total_height - margin_bottom - ((tick_value - min_y) / (max_y - min_y) * plot_height as f64) as u32;
// Variable declaration
        let tick_x = margin_left;
        
        // Draw tick mark
        for dx in 0..5 {
            if tick_x >= dx {
                img.put_pixel(tick_x - dx, y_pos, color);
            }
        }
        
        // Draw label
// Variable declaration
        let text = format_number(tick_value);
// Variable declaration
        let char_width = (6.0 * font_scale) as u32;
// Variable declaration
        let text_width = text.len() as u32 * char_width;
// Variable declaration
        let label_x = if tick_x >= text_width + 10 {
            tick_x - text_width - 10
        } else {
            0
        };
        
// Variable declaration
        let char_height = (7.0 * font_scale) as u32;
// Variable declaration
        let label_y = y_pos.saturating_sub(char_height / 2);
        
        draw_number_pixels_scaled(img, label_x, label_y, tick_value, color, font_scale);
    }
}

/// Function: explain its purpose and key arguments
fn draw_subplot_legend(
    img: &mut image::RgbImage,
    datasets: &[Dataset],
    legend_title: &str,
    x_offset: u32,
    y_offset: u32,
    color: image::Rgb<u8>,
    font_size: &FontSize,
) {
// Variable declaration
    let font_scale = font_size.to_scale();
// Variable declaration
    let line_height = (10.0 * font_scale) as u32;
// Variable declaration
    let mut current_y = y_offset;
    
    // Draw legend title if provided
    if !legend_title.is_empty() {
        draw_text_scaled(img, x_offset, current_y, legend_title, color, font_scale);
        current_y += line_height + 5;
    }
    
    // Draw legend entries
    for dataset in datasets.iter().take(5) { // Limit to 5 entries for space
        // Draw color square
// Variable declaration
        let square_size = (8.0 * font_scale) as u32;
// Variable declaration
        let dataset_color = image::Rgb(dataset.color);
        for dy in 0..square_size {
            for dx in 0..square_size {
                if x_offset + dx < img.width() && current_y + dy < img.height() {
                    img.put_pixel(x_offset + dx, current_y + dy, dataset_color);
                }
            }
        }
        
        // Draw dataset name (truncated if too long)
// Variable declaration
        let name = if dataset.name.len() > 15 {
            format!("{}...", &dataset.name[..12])
        } else {
            dataset.name.clone()
        };
        
        draw_text_scaled(img, x_offset + square_size + 5, current_y, &name, color, font_scale * 0.8);
        current_y += line_height;
    }
}

/// Function: explain its purpose and key arguments
fn draw_text_scaled(
    img: &mut image::RgbImage,
    x: u32,
    y: u32,
    text: &str,
    color: image::Rgb<u8>,
    scale: f32,
) {
// Variable declaration
    let char_width = (6.0 * scale) as u32;
    for (i, ch) in text.chars().enumerate() {
// Variable declaration
        let char_x = x + (i as u32 * char_width);
        draw_char_pixels_scaled(img, char_x, y, ch, color, scale);
    }
}

// Original single-plot export function (backward compatibility)
/// Function: explain its purpose and key arguments
pub fn export_plot_as_png_with_config(
    datasets: &[Dataset],
    dark_mode: bool,
    show_grid: bool,
    axis_config: Option<AxisConfig>,
    font_size: &FontSize,
) -> Result<(), Box<dyn std::error::Error>> {
    // Convert to subplot format for unified export
// Variable declaration
    let mut subplot = Subplot::new("single".to_string());
    subplot.datasets = datasets.to_vec();
    subplot.config.show_grid = show_grid;
    
    if let Some(config) = axis_config {
        subplot.config.use_custom_bounds = true;
        subplot.config.custom_x_min = config.x_min.map_or(String::new(), |v| v.to_string());
        subplot.config.custom_x_max = config.x_max.map_or(String::new(), |v| v.to_string());
        subplot.config.custom_y_min = config.y_min.map_or(String::new(), |v| v.to_string());
        subplot.config.custom_y_max = config.y_max.map_or(String::new(), |v| v.to_string());
        subplot.config.x_padding_percent = config.x_padding_percent * 100.0;
        subplot.config.y_padding_percent = config.y_padding_percent * 100.0;
        
        if let Some(x_ticks) = config.custom_x_ticks {
            subplot.config.use_custom_x_ticks = true;
            subplot.config.custom_x_ticks = x_ticks.iter()
                .map(|v| v.to_string())
                .collect::<Vec<_>>()
                .join(", ");
        }
        
        if let Some(y_ticks) = config.custom_y_ticks {
            subplot.config.use_custom_y_ticks = true;
            subplot.config.custom_y_ticks = y_ticks.iter()
                .map(|v| v.to_string())
                .collect::<Vec<_>>()
                .join(", ");
        }
    }
    
    export_subplots_as_png(&[subplot], &SubplotLayout::Single, dark_mode, font_size)
}

/// Function: explain its purpose and key arguments
pub fn calculate_custom_bounds(datasets: &[Dataset], config: &AxisConfig) -> Result<(f64, f64, f64, f64), Box<dyn std::error::Error>> {
// Variable declaration
    let (data_min_x, data_max_x, data_min_y, data_max_y) = get_data_bounds(datasets)
        .ok_or("No data available")?;

// Variable declaration
    let base_min_x = config.x_min.unwrap_or(data_min_x);
// Variable declaration
    let base_max_x = config.x_max.unwrap_or(data_max_x);
// Variable declaration
    let base_min_y = config.y_min.unwrap_or(data_min_y);
// Variable declaration
    let base_max_y = config.y_max.unwrap_or(data_max_y);

// Variable declaration
    let x_range = base_max_x - base_min_x;
// Variable declaration
    let y_range = base_max_y - base_min_y;

// Variable declaration
    let x_padding = x_range * config.x_padding_percent;
// Variable declaration
    let y_padding = y_range * config.y_padding_percent;

// Variable declaration
    let min_x = base_min_x - x_padding;
// Variable declaration
    let max_x = base_max_x + x_padding;
// Variable declaration
    let min_y = base_min_y - y_padding;
// Variable declaration
    let max_y = base_max_y + y_padding;

    Ok((min_x, max_x, min_y, max_y))
}

/// Function: explain its purpose and key arguments
pub fn calculate_auto_bounds(datasets: &[Dataset]) -> (f64, f64, f64, f64) {
// Variable declaration
    let (mut min_x, mut max_x, mut min_y, mut max_y) = get_data_bounds(datasets)
        .unwrap_or((0.0, 1.0, 0.0, 1.0));

    if (max_x - min_x).abs() < f64::EPSILON {
// Variable declaration
        let center = min_x;
        min_x = center - 1.0;
        max_x = center + 1.0;
    }

    if (max_y - min_y).abs() < f64::EPSILON {
// Variable declaration
        let center = min_y;
        min_y = center - 1.0;
        max_y = center + 1.0;
    }

// Variable declaration
    let x_range = max_x - min_x;
// Variable declaration
    let y_range = max_y - min_y;
// Variable declaration
    let padding_percent = 0.05;

// Variable declaration
    let x_padding = x_range * padding_percent;
// Variable declaration
    let y_padding = y_range * padding_percent;

// Variable declaration
    let padded_min_x = min_x - x_padding;
// Variable declaration
    let padded_min_y = if min_y > 0.0 {
        (min_y - y_padding).max(0.0)
    } else {
        min_y - y_padding
    };

    (padded_min_x, max_x + x_padding, padded_min_y, max_y + y_padding)
}

// Enhanced axis label drawing with custom ticks and font size support
/// Function: explain its purpose and key arguments
pub fn draw_axis_labels_with_custom_ticks_and_font(
    img: &mut image::RgbImage,
    min_x: f64,
    max_x: f64,
    min_y: f64,
    max_y: f64,
    margin_left: u32,
    margin_bottom: u32,
    plot_width: u32,
    plot_height: u32,
    width: u32,
    height: u32,
    color: image::Rgb<u8>,
    axis_config: Option<&AxisConfig>,
    font_size: &FontSize,
) {
// Variable declaration
    let font_scale = font_size.to_scale();
    
    // X-axis ticks and labels
// Variable declaration
    let x_tick_values: Vec<f64> = if let Some(config) = axis_config {
        if let Some(ref custom_x_ticks) = config.custom_x_ticks {
            // Use custom ticks, but filter to only those within range
            custom_x_ticks.iter()
                .filter(|&&tick| tick >= min_x && tick <= max_x)
                .copied()
                .collect()
        } else {
            // Use default 6 evenly spaced ticks
            (0..=6).map(|i| min_x + (max_x - min_x) * (i as f64 / 6.0)).collect()
        }
    } else {
        // Use default 6 evenly spaced ticks
        (0..=6).map(|i| min_x + (max_x - min_x) * (i as f64 / 6.0)).collect()
    };

    for &tick_value in &x_tick_values {
// Variable declaration
        let x_pos = margin_left + ((tick_value - min_x) / (max_x - min_x) * plot_width as f64) as u32;
// Variable declaration
        let tick_y = height - margin_bottom;
        
        // Draw tick mark
        for dy in 0..8 {
            if tick_y + dy < height {
                img.put_pixel(x_pos, tick_y + dy, color);
            }
        }
        
        // Draw label with font scaling
// Variable declaration
        let text = format_number(tick_value);
// Variable declaration
        let char_width = (6.0 * font_scale) as u32;
// Variable declaration
        let text_width = text.len() as u32 * char_width;
// Variable declaration
        let label_x = if x_pos >= text_width / 2 {
            x_pos - text_width / 2
        } else {
            0
        };
        
        draw_number_pixels_scaled(img, label_x, tick_y + 20, tick_value, color, font_scale);
    }

    // Y-axis ticks and labels
// Variable declaration
    let y_tick_values: Vec<f64> = if let Some(config) = axis_config {
        if let Some(ref custom_y_ticks) = config.custom_y_ticks {
            // Use custom ticks, but filter to only those within range
            custom_y_ticks.iter()
                .filter(|&&tick| tick >= min_y && tick <= max_y)
                .copied()
                .collect()
        } else {
            // Use default 6 evenly spaced ticks
            (0..=6).map(|i| min_y + (max_y - min_y) * (i as f64 / 6.0)).collect()
        }
    } else {
        // Use default 6 evenly spaced ticks
        (0..=6).map(|i| min_y + (max_y - min_y) * (i as f64 / 6.0)).collect()
    };

    for &tick_value in &y_tick_values {
// Variable declaration
        let y_pos = height - margin_bottom - ((tick_value - min_y) / (max_y - min_y) * plot_height as f64) as u32;
// Variable declaration
        let tick_x = margin_left;
        
        // Draw tick mark
        for dx in 0..8 {
            if tick_x >= dx {
                img.put_pixel(tick_x - dx, y_pos, color);
            }
        }
        
        // Draw label with font scaling
// Variable declaration
        let text = format_number(tick_value);
// Variable declaration
        let char_width = (6.0 * font_scale) as u32;
// Variable declaration
        let text_width = text.len() as u32 * char_width;
// Variable declaration
        let label_x = if tick_x >= text_width + 15 {
            tick_x - text_width - 15
        } else {
            0
        };
        
// Variable declaration
        let char_height = (7.0 * font_scale) as u32;
// Variable declaration
        let label_y = y_pos.saturating_sub(char_height / 2);
        
        draw_number_pixels_scaled(img, label_x, label_y, tick_value, color, font_scale);
    }
}

// Legacy function for backward compatibility - redirect to new function with medium font
/// Function: explain its purpose and key arguments
pub fn draw_axis_labels_with_custom_ticks(
    img: &mut image::RgbImage,
    min_x: f64,
    max_x: f64,
    min_y: f64,
    max_y: f64,
    margin_left: u32,
    margin_bottom: u32,
    plot_width: u32,
    plot_height: u32,
    width: u32,
    height: u32,
    color: image::Rgb<u8>,
    axis_config: Option<&AxisConfig>,
) {
// Variable declaration
    let font_size = FontSize::Medium;
    draw_axis_labels_with_custom_ticks_and_font(
        img, min_x, max_x, min_y, max_y, margin_left, margin_bottom,
        plot_width, plot_height, width, height, color, axis_config, &font_size
    );
}

// New function with font scaling support
/// Function: explain its purpose and key arguments
pub fn draw_number_pixels_scaled(
    img: &mut image::RgbImage,
    x: u32,
    y: u32,
    value: f64,
    color: image::Rgb<u8>,
    scale: f32,
) {
// Variable declaration
    let text = format_number(value);
// Variable declaration
    let char_width = (6.0 * scale) as u32;
    for (i, ch) in text.chars().enumerate() {
// Variable declaration
        let char_x = x + (i as u32 * char_width);
        draw_char_pixels_scaled(img, char_x, y, ch, color, scale);
    }
}

/// Function: explain its purpose and key arguments
pub fn draw_char_pixels_scaled(
    img: &mut image::RgbImage, 
    x: u32, 
    y: u32, 
    ch: char, 
    color: image::Rgb<u8>,
    scale: f32
) {
// Variable declaration
    let pattern = match ch {
        '0' => [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
        '1' => [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
        '2' => [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],
        '3' => [0b11111, 0b00010, 0b00100, 0b00110, 0b00001, 0b10001, 0b01110],
        '4' => [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],
        '5' => [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
        '6' => [0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
        '7' => [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
        '8' => [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
        '9' => [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100],
        '.' => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100],
        '-' => [0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000],
        'K' => [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
        'M' => [0b10001, 0b11011, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001],
        'e' => [0b00000, 0b01110, 0b10001, 0b11111, 0b10000, 0b10001, 0b01110],
        ' ' => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000],
        ':' => [0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000],
        'S' => [0b01110, 0b10001, 0b10000, 0b01110, 0b00001, 0b10001, 0b01110],
        'u' => [0b00000, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01111],
        'b' => [0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b10001, 0b11110],
        'p' => [0b00000, 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000],
        'l' => [0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
        'o' => [0b00000, 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
        't' => [0b00100, 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00011],
        _ => [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000],
    };

// Variable declaration
    let pixel_size = scale.max(1.0) as u32;

    for (row, &pattern_row) in pattern.iter().enumerate() {
        for col in 0..5 {
            if (pattern_row >> (4 - col)) & 1 == 1 {
                // Draw scaled pixel as a block
                for dy in 0..pixel_size {
                    for dx in 0..pixel_size {
// Variable declaration
                        let px = x + (col * pixel_size) + dx;
// Variable declaration
                        let py = y + (row as u32 * pixel_size) + dy;
                        if px < img.width() && py < img.height() {
                            img.put_pixel(px, py, color);
                        }
                    }
                }
            }
        }
    }
}

// Keep the original functions for backward compatibility
/// Function: explain its purpose and key arguments
pub fn draw_number_pixels(
    img: &mut image::RgbImage,
    x: u32,
    y: u32,
    value: f64,
    color: image::Rgb<u8>,
) {
    draw_number_pixels_scaled(img, x, y, value, color, 1.0);
}

/// Function: explain its purpose and key arguments
pub fn draw_char_pixels(img: &mut image::RgbImage, x: u32, y: u32, ch: char, color: image::Rgb<u8>) {
    draw_char_pixels_scaled(img, x, y, ch, color, 1.0);
}

/// Function: explain its purpose and key arguments
pub fn draw_thick_line(
    img: &mut image::RgbImage,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color: image::Rgb<u8>,
    thickness: u32,
) {
    for offset in 0..thickness {
// Variable declaration
        let offset = offset as i32 - (thickness as i32 / 2);
        draw_line_offset(img, x0, y0, x1, y1, color, offset, 0);
        if offset != 0 {
            draw_line_offset(img, x0, y0, x1, y1, color, 0, offset);
        }
    }
}

/// Function: explain its purpose and key arguments
pub fn draw_line_offset(
    img: &mut image::RgbImage,
    x0: u32,
    y0: u32,
    x1: u32,
    y1: u32,
    color: image::Rgb<u8>,
    offset_x: i32,
    offset_y: i32,
) {
// Variable declaration
    let dx = (x1 as i32 - x0 as i32).abs();
// Variable declaration
    let dy = (y1 as i32 - y0 as i32).abs();
// Variable declaration
    let sx = if x0 < x1 { 1 } else { -1 };
// Variable declaration
    let sy = if y0 < y1 { 1 } else { -1 };
// Variable declaration
    let mut err = dx - dy;
// Variable declaration
    let mut x = x0 as i32;
// Variable declaration
    let mut y = y0 as i32;

    loop {
// Variable declaration
        let px = x + offset_x;
// Variable declaration
        let py = y + offset_y;

        if px >= 0 && py >= 0 && (px as u32) < img.width() && (py as u32) < img.height() {
            img.put_pixel(px as u32, py as u32, color);
        }

        if x == x1 as i32 && y == y1 as i32 {
            break;
        }

// Variable declaration
        let e2 = 2 * err;
        if e2 > -dy {
            err -= dy;
            x += sx;
        }
        if e2 < dx {
            err += dx;
            y += sy;
        }
    }
}

/// Function: explain its purpose and key arguments
pub fn load_csv_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
// Variable declaration
    let mut rdr = csv::Reader::from_path(path)?;
// Variable declaration
    let mut out = Vec::new();
    for result in rdr.records() {
// Variable declaration
        let record = result?;
        if record.len() < 2 {
            continue;
        }
        if let (Ok(x), Ok(y)) = (
            record.get(0).unwrap().trim().parse::<f64>(),
            record.get(1).unwrap().trim().parse::<f64>(),
        ) {
            out.push([x, y]);
        }
    }
    Ok(out)
}

/// Function: explain its purpose and key arguments
pub fn load_xvg_points(path: &PathBuf) -> Result<Vec<[f64; 2]>, Box<dyn std::error::Error>> {
// Variable declaration
    let file = File::open(path)?;
// Variable declaration
    let reader = BufReader::new(file);
// Variable declaration
    let mut points = Vec::new();

    for line_result in reader.lines() {
// Variable declaration
        let line = line_result?;
// Variable declaration
        let line = line.trim();

        if line.is_empty() || line.starts_with('#') || line.starts_with('@') {
            continue;
        }

// Variable declaration
        let parts: Vec<&str> = line.split_whitespace().collect();

        if parts.len() < 2 {
            continue;
        }

        if let (Ok(x), Ok(y)) = (parts[0].parse::<f64>(), parts[1].parse::<f64>()) {
            points.push([x, y]);
        }
    }

    Ok(points)
}

/// Function: explain its purpose and key arguments
pub fn format_number(value: f64) -> String {
// Variable declaration
    let abs_value = value.abs();
    
    if abs_value >= 1_000_000.0 {
// Variable declaration
        let m_value = value / 1_000_000.0;
        format!("{:.1}M", m_value)
    } else if abs_value >= 100_000.0 {
// Variable declaration
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 10_000.0 {
// Variable declaration
        let k_value = value / 1000.0;
        format!("{:.0}K", k_value)
    } else if abs_value >= 1000.0 {
        format!("{:.0}", value)
    } else if abs_value >= 1.0 {
        if value.fract().abs() < 0.01 {
            format!("{:.0}", value)
        } else {
            format!("{:.1}", value)
        }
    } else if abs_value >= 0.01 {
        format!("{:.3}", value)
    } else if abs_value > f64::EPSILON {
        format!("{:.4}", value)
    } else {
        "0".to_string()
    }
}

// Get default color palette
/// Function: explain its purpose and key arguments
pub fn get_default_color(index: usize) -> [u8; 3] {
// Variable declaration
    let colors = [
        [31, 120, 180],   // Blue
        [255, 127, 14],   // Orange  
        [44, 160, 44],    // Green
        [214, 39, 40],    // Red
        [148, 103, 189],  // Purple
        [140, 86, 75],    // Brown
        [227, 119, 194],  // Pink
        [127, 127, 127],  // Gray
    ];
    colors[index % colors.len()]
}

/// Function: explain its purpose and key arguments
pub fn pick_file() -> Option<PathBuf> {
    rfd::FileDialog::new()
        .add_filter("csv", &["csv"])
        .add_filter("xvg", &["xvg"])
        .pick_file()
}
/// Function: explain its purpose and key arguments
pub fn pick_multiple_files() -> Option<Vec<PathBuf>> {
    rfd::FileDialog::new()
        .add_filter("Data files", &["csv", "xvg"])
        .add_filter("CSV", &["csv"])
        .add_filter("XVG", &["xvg"])
        .pick_files()
}
