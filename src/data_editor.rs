// Import external modules or crates needed in data_editor.rs
use crate::dataset::Dataset;
// Import external modules or crates needed in data_editor.rs
use crate::utils::get_default_color;
// Import external modules or crates needed in data_editor.rs
use eframe::egui;
// Import external modules or crates needed in data_editor.rs
use std::collections::HashMap;

#[derive(Debug, Clone)]
/// Data structure used in data_editor.rs module
pub struct DataCell {
    pub value: String,
    pub parsed_value: Option<f64>,
    pub is_header: bool,
}

/// Implementation block defining methods for this type
impl Default for DataCell {
/// Function: explain its purpose and key arguments
    fn default() -> Self {
        Self {
            value: String::new(),
            parsed_value: None,
            is_header: false,
        }
    }
}

#[derive(Debug, Clone)]
/// Data structure used in data_editor.rs module
pub struct SpreadsheetData {
    pub cells: HashMap<(usize, usize), DataCell>, // (row, col) -> cell
    pub num_rows: usize,
    pub num_cols: usize,
    pub column_headers: Vec<String>,
    pub dataset_columns: Vec<Option<usize>>, // Maps column index to dataset index
}

/// Implementation block defining methods for this type
impl Default for SpreadsheetData {
/// Function: explain its purpose and key arguments
    fn default() -> Self {
// Variable declaration
        let mut headers = Vec::new();
// Variable declaration
        let mut dataset_columns = Vec::new();
        for i in 0..10 {
            headers.push(format!("Col {}", i + 1));
            dataset_columns.push(None);
        }
        
        Self {
            cells: HashMap::new(),
            num_rows: 50,
            num_cols: 10,
            column_headers: headers,
            dataset_columns,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
/// Enum representing a set of related values in data_editor.rs module
pub enum FitModel {
    Linear,
    Sigmoid,
    Hill,
}

/// Implementation block defining methods for this type
impl FitModel {
/// Function: explain its purpose and key arguments
    pub fn to_string(&self) -> &'static str {
        match self {
            FitModel::Linear => "Linear (y = ax + b)",
            FitModel::Sigmoid => "Sigmoid (y = a / (1 + exp(-b(x-c))))",
            FitModel::Hill => "Hill (y = (a * x^n) / (k^n + x^n))",
        }
    }
}

#[derive(Debug, Clone)]
/// Data structure used in data_editor.rs module
pub struct FitResult {
    pub model: FitModel,
    pub parameters: Vec<f64>,
    pub parameter_names: Vec<String>,
    pub r_squared: f64,
    pub fitted_points: Vec<[f64; 2]>,
    pub equation_string: String,
}

#[derive(Debug, Clone, PartialEq)]
/// Enum representing a set of related values in data_editor.rs module
pub enum MouseAction {
    Select,
    Edit,
    Copy,
    Delete,
}

#[derive(Debug, Clone)]
/// Data structure used in data_editor.rs module
pub struct Selection {
    pub start_row: usize,
    pub start_col: usize,
    pub end_row: usize,
    pub end_col: usize,
}

/// Implementation block defining methods for this type
impl Selection {
/// Function: explain its purpose and key arguments
    pub fn new(row: usize, col: usize) -> Self {
        Self {
            start_row: row,
            start_col: col,
            end_row: row,
            end_col: col,
        }
    }

/// Function: explain its purpose and key arguments
    pub fn extend_to(&mut self, row: usize, col: usize) {
        self.end_row = row;
        self.end_col = col;
    }

/// Function: explain its purpose and key arguments
    pub fn contains(&self, row: usize, col: usize) -> bool {
// Variable declaration
        let min_row = self.start_row.min(self.end_row);
// Variable declaration
        let max_row = self.start_row.max(self.end_row);
// Variable declaration
        let min_col = self.start_col.min(self.end_col);
// Variable declaration
        let max_col = self.start_col.max(self.end_col);
        
        row >= min_row && row <= max_row && col >= min_col && col <= max_col
    }
}

#[derive(Debug, Clone)]
/// Data structure used in data_editor.rs module
pub struct DataEditor {
    pub show_editor: bool,
    pub spreadsheet_data: SpreadsheetData,
    pub selected_dataset_index: usize,
    pub current_dataset: Option<Dataset>,
    pub scroll_offset: (f32, f32),
    
    // Enhanced selection and mouse functionality
    pub current_selection: Option<Selection>,
    pub mouse_action: MouseAction,
    pub clipboard_data: Vec<Vec<String>>,
    pub is_dragging: bool,
    pub edit_mode_cell: Option<(usize, usize)>,
    
    // Multi-dataset support
    pub loaded_datasets: Vec<usize>, // Dataset indices that are loaded in columns
    pub auto_update_plots: bool,
    pub column_dataset_mapping: HashMap<usize, usize>, // column -> dataset_index
    
    // Data transformation
    pub show_transform_dialog: bool,
    pub selected_row: Option<usize>,
    pub transform_x_column: usize,
    pub transform_y_column: usize,
    pub new_dataset_name: String,
    pub paste_buffer: String,
    pub selected_x_data: Vec<f64>,
    
    // Curve fitting
    pub show_fitting_dialog: bool,
    pub selected_fit_model: FitModel,
    pub fit_results: Vec<FitResult>,
    pub fitting_dataset_index: usize,
    pub show_paste_dialog: bool,
}

/// Implementation block defining methods for this type
impl Default for DataEditor {
/// Function: explain its purpose and key arguments
    fn default() -> Self {
        Self {
            show_editor: false,
            spreadsheet_data: SpreadsheetData::default(),
            selected_dataset_index: 0,
            current_dataset: None,
            scroll_offset: (0.0, 0.0),
            current_selection: None,
            mouse_action: MouseAction::Select,
            clipboard_data: Vec::new(),
            is_dragging: false,
            edit_mode_cell: None,
            loaded_datasets: Vec::new(),
            auto_update_plots: true,
            column_dataset_mapping: HashMap::new(),
            show_transform_dialog: false,
            selected_row: None,
            transform_x_column: 0,
            transform_y_column: 1,
            new_dataset_name: "New Dataset".to_string(),
            paste_buffer: String::new(),
            selected_x_data: Vec::new(),
            show_fitting_dialog: false,
            selected_fit_model: FitModel::Linear,
            fit_results: Vec::new(),
            fitting_dataset_index: 0,
            show_paste_dialog: false,
        }
    }
}

/// Implementation block defining methods for this type
impl DataEditor {
/// Function: explain its purpose and key arguments
    pub fn show_data_editor_window(&mut self, ctx: &egui::Context, datasets: &mut Vec<Dataset>) {
        if !self.show_editor {
            return;
        }

        egui::Window::new("Enhanced Data Editor")
            .resizable(true)
            .default_width(900.0)
            .default_height(700.0)
            .show(ctx, |ui| {
                // Top controls
                ui.horizontal(|ui| {
                    ui.label("Mouse Mode:");
                    ui.radio_value(&mut self.mouse_action, MouseAction::Select, "Select");
                    ui.radio_value(&mut self.mouse_action, MouseAction::Edit, "Edit");
                    ui.radio_value(&mut self.mouse_action, MouseAction::Copy, "Copy");
                    ui.radio_value(&mut self.mouse_action, MouseAction::Delete, "Delete");
                    
                    ui.separator();
                    
                    ui.checkbox(&mut self.auto_update_plots, "Auto-update plots");
                    
                    if ui.button("Update Plots Now").clicked() {
                        self.update_datasets_from_spreadsheet(datasets);
                    }
                });
                
                ui.separator();
                
                // Dataset loading controls
                ui.horizontal(|ui| {
                    ui.label("Load Multiple Datasets:");
                    
                    if !datasets.is_empty() {
                        for (i, dataset) in datasets.iter().enumerate() {
// Variable declaration
                            let is_loaded = self.loaded_datasets.contains(&i);
// Variable declaration
                            let mut should_load = is_loaded;
                            
                            if ui.checkbox(&mut should_load, &dataset.name).changed() {
                                if should_load && !is_loaded {
                                    self.load_dataset_to_column(datasets, i);
                                } else if !should_load && is_loaded {
                                    self.unload_dataset_from_columns(i);
                                }
                            }
                        }
                    }
                    
                    if ui.button("Clear All").clicked() {
                        self.clear_all_data();
                    }
                });
                
                ui.separator();
                
                // Action buttons
                ui.horizontal(|ui| {
                    if ui.button("📊 Transform Row to Dataset").clicked() {
                        self.show_transform_dialog = true;
                    }
                    
                    if ui.button("📈 Fit Curve").clicked() {
                        self.show_fitting_dialog = true;
                    }
                    
                    if ui.button("📋 Paste Data").clicked() {
                        self.show_paste_dialog = !self.show_paste_dialog;
                    }
                    
                    if ui.button("+ Add Row").clicked() {
                        self.add_row();
                    }
                    
                    if ui.button("+ Add Column").clicked() {
                        self.add_column();
                    }
                    
                    // Selection operations
                    ui.separator();
                    if self.current_selection.is_some() {
                        if ui.button("Copy Selection").clicked() {
                            self.copy_selection();
                        }
                        
                        if ui.button("Paste Here").clicked() {
                            self.paste_at_selection();
                        }
                        
                        if ui.button("Clear Selection").clicked() {
                            self.clear_selection();
                        }
                    }
                });
                
                // Paste dialog
                if self.show_paste_dialog {
                    ui.separator();
                    ui.label("Paste tab-separated or comma-separated data:");
                    ui.text_edit_multiline(&mut self.paste_buffer);
                    
                    ui.horizontal(|ui| {
                        if ui.button("Parse and Insert").clicked() {
                            self.parse_pasted_data();
                            self.show_paste_dialog = false;
                            if self.auto_update_plots {
                                self.update_datasets_from_spreadsheet(datasets);
                            }
                        }
                        
                        if ui.button("Cancel").clicked() {
                            self.show_paste_dialog = false;
                            self.paste_buffer.clear();
                        }
                    });
                }
                
                // Selection info
                if let Some(selection) = &self.current_selection {
                    ui.label(format!(
                        "Selection: ({},{}) to ({},{})", 
                        selection.start_row + 1, selection.start_col + 1,
                        selection.end_row + 1, selection.end_col + 1
                    ));
                }
                
                ui.separator();
                
                // Enhanced spreadsheet view
                egui::ScrollArea::both()
                    .max_height(400.0)
                    .show(ui, |ui| {
                        self.show_enhanced_spreadsheet(ui, datasets);
                    });
            });
        
        // Show dialogs
        self.show_transform_dialog_window(ctx, datasets);
        self.show_fitting_dialog_window(ctx, datasets);
    }
    
/// Function: explain its purpose and key arguments
    fn show_enhanced_spreadsheet(&mut self, ui: &mut egui::Ui, datasets: &mut Vec<Dataset>) {
        egui::Grid::new("enhanced_data_spreadsheet")
            .num_columns(self.spreadsheet_data.num_cols + 1)
            .spacing([2.0, 2.0])
            .striped(true)
            .show(ui, |ui| {
                // Header row with dataset indicators
                ui.label("Row");
// Variable declaration
                let mut header_updates = Vec::new();
                
                for (col_idx, header) in self.spreadsheet_data.column_headers.iter().enumerate() {
                    if col_idx >= self.spreadsheet_data.num_cols {
                        break;
                    }
                    
                    ui.vertical(|ui| {
                        // Dataset indicator
                        if let Some(&dataset_idx) = self.column_dataset_mapping.get(&col_idx) {
                            if let Some(dataset) = datasets.get(dataset_idx) {
                                ui.colored_label(
                                    egui::Color32::from_rgb(dataset.color[0], dataset.color[1], dataset.color[2]),
                                    format!("📊 {}", dataset.name)
                                );
                            }
                        }
                        
                        // Column header
// Variable declaration
                        let mut header_text = header.clone();
                        if ui.text_edit_singleline(&mut header_text).changed() {
                            header_updates.push((col_idx, header_text));
                        }
                    });
                }
                
                // Apply header updates
                for (col_idx, new_header) in header_updates {
                    self.spreadsheet_data.column_headers[col_idx] = new_header;
                }
                
                ui.end_row();
                
                // Data rows with enhanced mouse interaction
                for row in 0..self.spreadsheet_data.num_rows {
                    // Row header
// Variable declaration
                    let row_selected = self.current_selection.as_ref()
                        .map_or(false, |sel| sel.contains(row, 0));
                    
// Variable declaration
                    let row_header_response = ui.selectable_label(
                        row_selected,
                        format!("{}", row + 1)
                    );
                    
                    if row_header_response.clicked() {
                        self.handle_row_header_click(row);
                    }
                    
                    // Data cells with enhanced interaction
                    for col in 0..self.spreadsheet_data.num_cols {
// Variable declaration
                        let cell_key = (row, col);
// Variable declaration
                        let mut cell = self.spreadsheet_data.cells
                            .get(&cell_key)
                            .cloned()
                            .unwrap_or_default();
                        
// Variable declaration
                        let is_selected = self.current_selection.as_ref()
                            .map_or(false, |sel| sel.contains(row, col));
                        
// Variable declaration
                        let is_editing = self.edit_mode_cell == Some((row, col));
                        
                        // Visual styling for selection
// Variable declaration
                        let mut response = if is_editing {
                            ui.text_edit_singleline(&mut cell.value)
                        } else if is_selected {
                            ui.colored_label(egui::Color32::LIGHT_BLUE, &cell.value)
                                .on_hover_text("Selected cell")
                        } else {
                            ui.label(&cell.value)
                        };
                        
                        // Handle mouse interactions
                        if response.clicked() {
                            self.handle_cell_click(row, col);
                        }
                        
                        if response.drag_started() {
                            self.start_drag_selection(row, col);
                        }
                        
                        if response.dragged() && self.is_dragging {
                            self.extend_drag_selection(row, col);
                        }
                        
                        if response.drag_stopped() {
                            self.end_drag_selection();
                        }
                        
                        // Update cell data if changed
                        if response.changed() {
                            cell.parsed_value = cell.value.trim().parse::<f64>().ok();
                            self.spreadsheet_data.cells.insert(cell_key, cell.clone());
                            
                            // Auto-update plots if enabled
                            if self.auto_update_plots {
                                self.update_datasets_from_spreadsheet(datasets);
                            }
                        }
                        
                        // Visual feedback for parsing errors
                        if !cell.value.is_empty() && cell.parsed_value.is_none() {
                            ui.colored_label(egui::Color32::RED, "!");
                        }
                        
                        // Show dataset mapping indicator
                        if self.column_dataset_mapping.contains_key(&col) {
                            ui.colored_label(egui::Color32::GREEN, "●");
                        }
                    }
                    ui.end_row();
                }
            });
    }
    
/// Function: explain its purpose and key arguments
    fn handle_cell_click(&mut self, row: usize, col: usize) {
        match self.mouse_action {
            MouseAction::Select => {
                self.current_selection = Some(Selection::new(row, col));
                self.edit_mode_cell = None;
            },
            MouseAction::Edit => {
                self.edit_mode_cell = Some((row, col));
                self.current_selection = Some(Selection::new(row, col));
            },
            MouseAction::Copy => {
                if let Some(selection) = &self.current_selection {
                    if selection.contains(row, col) {
                        self.copy_selection();
                    }
                } else {
                    self.current_selection = Some(Selection::new(row, col));
                    self.copy_selection();
                }
            },
            MouseAction::Delete => {
                if let Some(selection) = &self.current_selection {
                    if selection.contains(row, col) {
                        self.clear_selection();
                    }
                } else {
                    self.spreadsheet_data.cells.remove(&(row, col));
                }
            },
        }
    }
    
/// Function: explain its purpose and key arguments
    fn handle_row_header_click(&mut self, row: usize) {
        // Select entire row
        self.current_selection = Some(Selection {
            start_row: row,
            start_col: 0,
            end_row: row,
            end_col: self.spreadsheet_data.num_cols - 1,
        });
    }
    
/// Function: explain its purpose and key arguments
    fn start_drag_selection(&mut self, row: usize, col: usize) {
        self.is_dragging = true;
        if self.current_selection.is_none() {
            self.current_selection = Some(Selection::new(row, col));
        }
    }
    
/// Function: explain its purpose and key arguments
    fn extend_drag_selection(&mut self, row: usize, col: usize) {
        if let Some(selection) = &mut self.current_selection {
            selection.extend_to(row, col);
        }
    }
    
/// Function: explain its purpose and key arguments
    fn end_drag_selection(&mut self) {
        self.is_dragging = false;
    }
    
/// Function: explain its purpose and key arguments
    fn copy_selection(&mut self) {
        if let Some(selection) = &self.current_selection {
// Variable declaration
            let min_row = selection.start_row.min(selection.end_row);
// Variable declaration
            let max_row = selection.start_row.max(selection.end_row);
// Variable declaration
            let min_col = selection.start_col.min(selection.end_col);
// Variable declaration
            let max_col = selection.start_col.max(selection.end_col);
            
            self.clipboard_data.clear();
            
            for row in min_row..=max_row {
// Variable declaration
                let mut row_data = Vec::new();
                for col in min_col..=max_col {
// Variable declaration
                    let cell = self.spreadsheet_data.cells.get(&(row, col));
                    row_data.push(cell.map_or(String::new(), |c| c.value.clone()));
                }
                self.clipboard_data.push(row_data);
            }
        }
    }
    
/// Function: explain its purpose and key arguments
    fn paste_at_selection(&mut self) {
        if let Some(selection) = &self.current_selection {
// Variable declaration
            let start_row = selection.start_row;
// Variable declaration
            let start_col = selection.start_col;
            
            for (row_offset, row_data) in self.clipboard_data.iter().enumerate() {
                for (col_offset, cell_value) in row_data.iter().enumerate() {
// Variable declaration
                    let target_row = start_row + row_offset;
// Variable declaration
                    let target_col = start_col + col_offset;
                    
                    if target_row < self.spreadsheet_data.num_rows && 
                       target_col < self.spreadsheet_data.num_cols {
// Variable declaration
                        let mut cell = DataCell::default();
                        cell.value = cell_value.clone();
                        cell.parsed_value = cell.value.parse::<f64>().ok();
                        
                        self.spreadsheet_data.cells.insert((target_row, target_col), cell);
                    }
                }
            }
        }
    }
    
/// Function: explain its purpose and key arguments
    fn clear_selection(&mut self) {
        if let Some(selection) = &self.current_selection {
// Variable declaration
            let min_row = selection.start_row.min(selection.end_row);
// Variable declaration
            let max_row = selection.start_row.max(selection.end_row);
// Variable declaration
            let min_col = selection.start_col.min(selection.end_col);
// Variable declaration
            let max_col = selection.start_col.max(selection.end_col);
            
            for row in min_row..=max_row {
                for col in min_col..=max_col {
                    self.spreadsheet_data.cells.remove(&(row, col));
                }
            }
        }
    }
    
/// Function: explain its purpose and key arguments
    fn load_dataset_to_column(&mut self, datasets: &[Dataset], dataset_idx: usize) {
        if let Some(dataset) = datasets.get(dataset_idx) {
            // Find next available column pair (X, Y)
// Variable declaration
            let mut target_col = 0;
            while self.column_dataset_mapping.contains_key(&target_col) || 
                  self.column_dataset_mapping.contains_key(&(target_col + 1)) {
                target_col += 2;
            }
            
            // Expand columns if needed
            while target_col + 1 >= self.spreadsheet_data.num_cols {
                self.add_column();
            }
            
            // Set column headers
            self.spreadsheet_data.column_headers[target_col] = format!("{}_X", dataset.name);
            self.spreadsheet_data.column_headers[target_col + 1] = format!("{}_Y", dataset.name);
            
            // Map columns to dataset
            self.column_dataset_mapping.insert(target_col, dataset_idx);
            self.column_dataset_mapping.insert(target_col + 1, dataset_idx);
            
            // Load data
            for (row, point) in dataset.points.iter().enumerate() {
                // Expand rows if needed
                if row >= self.spreadsheet_data.num_rows {
                    self.spreadsheet_data.num_rows = row + 1;
                }
                
                // X value
// Variable declaration
                let mut x_cell = DataCell::default();
                x_cell.value = point[0].to_string();
                x_cell.parsed_value = Some(point[0]);
                self.spreadsheet_data.cells.insert((row, target_col), x_cell);
                
                // Y value
// Variable declaration
                let mut y_cell = DataCell::default();
                y_cell.value = point[1].to_string();
                y_cell.parsed_value = Some(point[1]);
                self.spreadsheet_data.cells.insert((row, target_col + 1), y_cell);
            }
            
            self.loaded_datasets.push(dataset_idx);
        }
    }
    
/// Function: explain its purpose and key arguments
    fn unload_dataset_from_columns(&mut self, dataset_idx: usize) {
        // Find and remove columns associated with this dataset
// Variable declaration
        let mut cols_to_remove = Vec::new();
        for (&col, &mapped_dataset) in &self.column_dataset_mapping {
            if mapped_dataset == dataset_idx {
                cols_to_remove.push(col);
            }
        }
        
        // Clear data from those columns
        for col in cols_to_remove {
            self.column_dataset_mapping.remove(&col);
            for row in 0..self.spreadsheet_data.num_rows {
                self.spreadsheet_data.cells.remove(&(row, col));
            }
        }
        
        // Remove from loaded datasets
        self.loaded_datasets.retain(|&idx| idx != dataset_idx);
    }
    
/// Function: explain its purpose and key arguments
    fn update_datasets_from_spreadsheet(&mut self, datasets: &mut Vec<Dataset>) {
        // Group columns by dataset
// Variable declaration
        let mut dataset_columns: HashMap<usize, Vec<usize>> = HashMap::new();
        for (&col, &dataset_idx) in &self.column_dataset_mapping {
            dataset_columns.entry(dataset_idx).or_default().push(col);
        }
        
        // Update each dataset
        for (&dataset_idx, cols) in &dataset_columns {
            if let Some(dataset) = datasets.get_mut(dataset_idx) {
// Variable declaration
                let mut new_points = Vec::new();
                
                // Assume X column comes first, Y second
                if cols.len() >= 2 {
// Variable declaration
                    let mut sorted_cols = cols.clone();
                    sorted_cols.sort();
// Variable declaration
                    let x_col = sorted_cols[0];
// Variable declaration
                    let y_col = sorted_cols[1];
                    
                    // Collect data from spreadsheet
                    for row in 0..self.spreadsheet_data.num_rows {
                        if let (Some(x_cell), Some(y_cell)) = (
                            self.spreadsheet_data.cells.get(&(row, x_col)),
                            self.spreadsheet_data.cells.get(&(row, y_col))
                        ) {
                            if let (Some(x), Some(y)) = (x_cell.parsed_value, y_cell.parsed_value) {
                                new_points.push([x, y]);
                            }
                        }
                    }
                }
                
                dataset.points = new_points;
            }
        }
    }
    
/// Function: explain its purpose and key arguments
    fn add_row(&mut self) {
        self.spreadsheet_data.num_rows += 1;
    }
    
/// Function: explain its purpose and key arguments
    fn add_column(&mut self) {
        self.spreadsheet_data.num_cols += 1;
// Variable declaration
        let new_col_idx = self.spreadsheet_data.column_headers.len();
        self.spreadsheet_data.column_headers.push(format!("Col {}", new_col_idx + 1));
        self.spreadsheet_data.dataset_columns.push(None);
    }
    
/// Function: explain its purpose and key arguments
    fn clear_all_data(&mut self) {
        self.spreadsheet_data.cells.clear();
        self.column_dataset_mapping.clear();
        self.loaded_datasets.clear();
        self.current_selection = None;
        self.edit_mode_cell = None;
    }
    
/// Function: explain its purpose and key arguments
    fn parse_pasted_data(&mut self) {
        // Clone the buffer to avoid borrowing conflicts
// Variable declaration
        let buffer_content = self.paste_buffer.clone();
// Variable declaration
        let lines: Vec<&str> = buffer_content.lines().collect();
        
// Variable declaration
        let start_row = self.current_selection.as_ref().map_or(0, |sel| sel.start_row);
// Variable declaration
        let start_col = self.current_selection.as_ref().map_or(0, |sel| sel.start_col);
        
        // First pass: determine required dimensions
// Variable declaration
        let mut max_col_needed = 0;
// Variable declaration
        let mut max_row_needed = 0;
        
        for (row_offset, line) in lines.iter().enumerate() {
// Variable declaration
            let cells: Vec<&str> = if line.contains('\t') {
                line.split('\t').collect()
            } else {
                line.split(',').collect()
            };
            
// Variable declaration
            let target_row = start_row + row_offset;
// Variable declaration
            let target_col = start_col + cells.len().saturating_sub(1);
            
            max_row_needed = max_row_needed.max(target_row);
            max_col_needed = max_col_needed.max(target_col);
        }
        
        // Expand grid if necessary
        while self.spreadsheet_data.num_cols <= max_col_needed {
            self.add_column();
        }
        
        if max_row_needed >= self.spreadsheet_data.num_rows {
            self.spreadsheet_data.num_rows = max_row_needed + 1;
        }
        
        // Second pass: insert data
        for (row_offset, line) in lines.iter().enumerate() {
// Variable declaration
            let cells: Vec<&str> = if line.contains('\t') {
                line.split('\t').collect()
            } else {
                line.split(',').collect()
            };
            
            for (col_offset, cell_value) in cells.iter().enumerate() {
// Variable declaration
                let target_row = start_row + row_offset;
// Variable declaration
                let target_col = start_col + col_offset;
                
// Variable declaration
                let mut cell = DataCell::default();
                cell.value = cell_value.trim().to_string();
                cell.parsed_value = cell.value.parse::<f64>().ok();
                
                self.spreadsheet_data.cells.insert((target_row, target_col), cell);
            }
        }
        
        self.paste_buffer.clear();
    }
/// Function: explain its purpose and key arguments
    fn show_transform_dialog_window(&mut self, ctx: &egui::Context, datasets: &mut Vec<Dataset>) {
        if !self.show_transform_dialog {
            return;
        }

        egui::Window::new("Transform Row to Dataset")
            .resizable(true)
            .default_width(400.0)
            .show(ctx, |ui| {
                ui.heading("Create Dataset from Row");
                ui.separator();

                if let Some(selected_row) = self.selected_row {
                    ui.label(format!("Selected Row: {}", selected_row + 1));

                    ui.horizontal(|ui| {
                        ui.label("X Column:");
                        egui::ComboBox::from_label("")
                            .selected_text(&self.spreadsheet_data.column_headers[self.transform_x_column])
                            .show_ui(ui, |ui| {
                                for (i, header) in self.spreadsheet_data.column_headers.iter().enumerate() {
                                    if i < self.spreadsheet_data.num_cols {
                                        ui.selectable_value(&mut self.transform_x_column, i, header);
                                    }
                                }
                            });
                    });

                    ui.horizontal(|ui| {
                        ui.label("Y Column:");
                        egui::ComboBox::from_label("")
                            .selected_text(&self.spreadsheet_data.column_headers[self.transform_y_column])
                            .show_ui(ui, |ui| {
                                for (i, header) in self.spreadsheet_data.column_headers.iter().enumerate() {
                                    if i < self.spreadsheet_data.num_cols {
                                        ui.selectable_value(&mut self.transform_y_column, i, header);
                                    }
                                }
                            });
                    });

                    ui.horizontal(|ui| {
                        ui.label("Dataset Name:");
                        ui.text_edit_singleline(&mut self.new_dataset_name);
                    });

                    ui.separator();
                    ui.label("Paste X data (one value per line):");
                    ui.text_edit_multiline(&mut self.paste_buffer);

                    if ui.button("Parse X Data").clicked() {
                        self.selected_x_data = self.paste_buffer
                            .lines()
                            .filter_map(|line| line.trim().parse::<f64>().ok())
                            .collect();
                        self.paste_buffer.clear();
                    }

                    if !self.selected_x_data.is_empty() {
                        ui.label(format!("Parsed {} X values", self.selected_x_data.len()));
                    }

                    ui.horizontal(|ui| {
                        if ui.button("Create Dataset").clicked() {
                            self.create_dataset_from_row(datasets, selected_row);
                        }

                        if ui.button("Cancel").clicked() {
                            self.show_transform_dialog = false;
                            self.selected_x_data.clear();
                        }
                    });
                } else {
                    ui.label("Please select a row first by clicking on the row number in the editor.");
                    if ui.button("Close").clicked() {
                        self.show_transform_dialog = false;
                    }
                }
            });
    }

/// Function: explain its purpose and key arguments
    fn create_dataset_from_row(&mut self, datasets: &mut Vec<Dataset>, row: usize) {
// Variable declaration
        let mut points = Vec::new();

        if !self.selected_x_data.is_empty() {
            // Use provided X data and row data as Y values
// Variable declaration
            let mut y_values = Vec::new();
            for col in 0..self.spreadsheet_data.num_cols {
                if let Some(cell) = self.spreadsheet_data.cells.get(&(row, col)) {
                    if let Some(y_value) = cell.parsed_value {
                        y_values.push(y_value);
                    }
                }
            }

            // Pair X data with Y values
            for (i, &x_val) in self.selected_x_data.iter().enumerate() {
                if i < y_values.len() {
                    points.push([x_val, y_values[i]]);
                }
            }
        } else {
            // Use columns as X-Y pairs
            if let (Some(x_cell), Some(y_cell)) = (
                self.spreadsheet_data.cells.get(&(row, self.transform_x_column)),
                self.spreadsheet_data.cells.get(&(row, self.transform_y_column))
            ) {
                if let (Some(x), Some(y)) = (x_cell.parsed_value, y_cell.parsed_value) {
                    points.push([x, y]);
                }
            }
        }

        if !points.is_empty() {
// Variable declaration
            let color = get_default_color(datasets.len() % 8);
// Variable declaration
            let dataset = Dataset {
                name: self.new_dataset_name.clone(),
                points,
                color,
            };
            datasets.push(dataset);

            self.show_transform_dialog = false;
            self.selected_x_data.clear();
        }
    }

/// Function: explain its purpose and key arguments
    fn show_fitting_dialog_window(&mut self, ctx: &egui::Context, datasets: &mut Vec<Dataset>) {
        if !self.show_fitting_dialog {
            return;
        }

        egui::Window::new("Curve Fitting")
            .resizable(true)
            .default_width(500.0)
            .default_height(400.0)
            .show(ctx, |ui| {
                ui.heading("Fit Mathematical Models");
                ui.separator();

                // Dataset selection
                ui.horizontal(|ui| {
                    ui.label("Dataset:");
                    if !datasets.is_empty() {
                        egui::ComboBox::from_id_source("fit_dataset_combo")
                            .selected_text(
                                datasets.get(self.fitting_dataset_index)
                                    .map_or("None", |d| &d.name)
                            )
                            .show_ui(ui, |ui| {
                                for (i, dataset) in datasets.iter().enumerate() {
                                    ui.selectable_value(&mut self.fitting_dataset_index, i, &dataset.name);
                                }
                            });
                    }
                });

                // Model selection
                ui.horizontal(|ui| {
                    ui.label("Model:");
                    egui::ComboBox::from_id_source("fit_model_combo")
                        .selected_text(self.selected_fit_model.to_string())
                        .show_ui(ui, |ui| {
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Linear, FitModel::Linear.to_string());
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Sigmoid, FitModel::Sigmoid.to_string());
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Hill, FitModel::Hill.to_string());
                        });
                });

                ui.separator();

                // Fit button
                if ui.button("ðŸ”¬ Perform Fit").clicked() {
                    if let Some(dataset) = datasets.get(self.fitting_dataset_index) {
                        if let Some(fit_result) = self.perform_curve_fit(dataset) {
                            self.fit_results.push(fit_result.clone());

                            // Add fitted curve as new dataset
// Variable declaration
                            let fitted_dataset = Dataset {
                                name: format!("{}_fitted", dataset.name),
                                points: fit_result.fitted_points,
                                color: get_default_color((datasets.len() + 1) % 8),
                            };
                            datasets.push(fitted_dataset);
                        }
                    }
                }

                ui.separator();

                // Results display
                ui.heading("Fit Results:");
                egui::ScrollArea::vertical()
                    .max_height(200.0)
                    .show(ui, |ui| {
                        for (i, result) in self.fit_results.iter().enumerate() {
                            ui.group(|ui| {
                                ui.label(format!("Fit {}: {}", i + 1, result.model.to_string()));
                                ui.label(format!("RÂ² = {:.4}", result.r_squared));
                                ui.label(&result.equation_string);

                                for (param_name, param_value) in result.parameter_names.iter().zip(&result.parameters) {
                                    ui.label(format!("{} = {:.6}", param_name, param_value));
                                }
                            });
                        }
                    });

                ui.horizontal(|ui| {
                    if ui.button("Clear Results").clicked() {
                        self.fit_results.clear();
                    }

                    if ui.button("Close").clicked() {
                        self.show_fitting_dialog = false;
                    }
                });
            });
    }

/// Function: explain its purpose and key arguments
    fn perform_curve_fit(&self, dataset: &Dataset) -> Option<FitResult> {
        if dataset.points.len() < 3 {
            return None; // Need at least 3 points for fitting
        }

        match self.selected_fit_model {
            FitModel::Linear => self.fit_linear(dataset),
            FitModel::Sigmoid => self.fit_sigmoid(dataset),
            FitModel::Hill => self.fit_hill(dataset),
        }
    }

/// Function: explain its purpose and key arguments
    fn fit_linear(&self, dataset: &Dataset) -> Option<FitResult> {
// Variable declaration
        let n = dataset.points.len() as f64;
// Variable declaration
        let sum_x: f64 = dataset.points.iter().map(|p| p[0]).sum();
// Variable declaration
        let sum_y: f64 = dataset.points.iter().map(|p| p[1]).sum();
// Variable declaration
        let sum_xy: f64 = dataset.points.iter().map(|p| p[0] * p[1]).sum();
// Variable declaration
        let sum_x2: f64 = dataset.points.iter().map(|p| p[0] * p[0]).sum();

// Variable declaration
        let slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
// Variable declaration
        let intercept = (sum_y - slope * sum_x) / n;

        // Calculate RÂ²
// Variable declaration
        let y_mean = sum_y / n;
// Variable declaration
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
// Variable declaration
        let ss_res: f64 = dataset.points.iter().map(|p| {
// Variable declaration
            let y_pred = slope * p[0] + intercept;
            (p[1] - y_pred).powi(2)
        }).sum();

// Variable declaration
        let r_squared = 1.0 - (ss_res / ss_tot);

        // Generate fitted points
// Variable declaration
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min);
// Variable declaration
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);

// Variable declaration
        let mut fitted_points = Vec::new();
        for i in 0..100 {
// Variable declaration
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
// Variable declaration
            let y = slope * x + intercept;
            fitted_points.push([x, y]);
        }

        Some(FitResult {
            model: FitModel::Linear,
            parameters: vec![slope, intercept],
            parameter_names: vec!["slope".to_string(), "intercept".to_string()],
            r_squared,
            fitted_points,
            equation_string: format!("y = {:.4}x + {:.4}", slope, intercept),
        })
    }

/// Function: explain its purpose and key arguments
    fn fit_sigmoid(&self, dataset: &Dataset) -> Option<FitResult> {
        // Simplified sigmoid fitting using linearization
// Variable declaration
        let y_min = dataset.points.iter().map(|p| p[1]).fold(f64::INFINITY, f64::min);
// Variable declaration
        let y_max = dataset.points.iter().map(|p| p[1]).fold(f64::NEG_INFINITY, f64::max);

// Variable declaration
        let a = y_max - y_min;
// Variable declaration
        let y_offset = y_min;

        // Find approximate inflection point
// Variable declaration
        let x_mid = dataset.points.iter().map(|p| p[0]).sum::<f64>() / dataset.points.len() as f64;

        // Rough parameter estimates
// Variable declaration
        let b = 1.0; // steepness
// Variable declaration
        let c = x_mid; // inflection point

        // Generate fitted points
// Variable declaration
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min);
// Variable declaration
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);

// Variable declaration
        let mut fitted_points = Vec::new();
        for i in 0..100 {
// Variable declaration
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
// Variable declaration
            let y = y_offset + a / (1.0 + (-b * (x - c)).exp());
            fitted_points.push([x, y]);
        }

        // Calculate RÂ²
// Variable declaration
        let y_mean = dataset.points.iter().map(|p| p[1]).sum::<f64>() / dataset.points.len() as f64;
// Variable declaration
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
// Variable declaration
        let ss_res: f64 = dataset.points.iter().map(|p| {
// Variable declaration
            let y_pred = y_offset + a / (1.0 + (-b * (p[0] - c)).exp());
            (p[1] - y_pred).powi(2)
        }).sum();

// Variable declaration
        let r_squared = 1.0 - (ss_res / ss_tot);

        Some(FitResult {
            model: FitModel::Sigmoid,
            parameters: vec![a, b, c, y_offset],
            parameter_names: vec!["amplitude".to_string(), "steepness".to_string(), "inflection".to_string(), "offset".to_string()],
            r_squared,
            fitted_points,
            equation_string: format!("y = {:.4} + {:.4} / (1 + exp(-{:.4}(x - {:.4})))", y_offset, a, b, c),
        })
    }

/// Function: explain its purpose and key arguments
    fn fit_hill(&self, dataset: &Dataset) -> Option<FitResult> {
        // Simplified Hill equation fitting
        // y = (a * x^n) / (k^n + x^n)

// Variable declaration
        let y_max = dataset.points.iter().map(|p| p[1]).fold(f64::NEG_INFINITY, f64::max);
// Variable declaration
        let a = y_max; // maximum response

        // Find approximate K (half-maximal concentration)
// Variable declaration
        let half_max = a / 2.0;
// Variable declaration
        let k = dataset.points.iter()
            .min_by(|p1, p2| (p1[1] - half_max).abs().partial_cmp(&(p2[1] - half_max).abs()).unwrap())
            .map(|p| p[0])
            .unwrap_or(1.0);

// Variable declaration
        let n = 2.0; // Hill coefficient (cooperativity)

        // Generate fitted points
// Variable declaration
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min).max(0.001);
// Variable declaration
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);

// Variable declaration
        let mut fitted_points = Vec::new();
        for i in 0..100 {
// Variable declaration
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
            if x > 0.0 {
// Variable declaration
                let y = (a * x.powf(n)) / (k.powf(n) + x.powf(n));
                fitted_points.push([x, y]);
            }
        }

        // Calculate RÂ²
// Variable declaration
        let y_mean = dataset.points.iter().map(|p| p[1]).sum::<f64>() / dataset.points.len() as f64;
// Variable declaration
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
// Variable declaration
        let ss_res: f64 = dataset.points.iter().map(|p| {
// Variable declaration
            let y_pred = if p[0] > 0.0 {
                (a * p[0].powf(n)) / (k.powf(n) + p[0].powf(n))
            } else {
                0.0
            };
            (p[1] - y_pred).powi(2)
        }).sum();

// Variable declaration
        let r_squared = 1.0 - (ss_res / ss_tot);

        Some(FitResult {
            model: FitModel::Hill,
            parameters: vec![a, k, n],
            parameter_names: vec!["max_response".to_string(), "k_half".to_string(), "hill_coeff".to_string()],
            r_squared,
            fitted_points,
            equation_string: format!("y = ({:.4} * x^{:.2}) / ({:.4}^{:.2} + x^{:.2})", a, n, k, n, n),
        })
    }
}