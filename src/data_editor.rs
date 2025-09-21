use crate::dataset::Dataset;
use crate::utils::get_default_color;
use eframe::egui;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct DataCell {
    pub value: String,
    pub parsed_value: Option<f64>,
}

impl Default for DataCell {
    fn default() -> Self {
        Self {
            value: String::new(),
            parsed_value: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct SpreadsheetData {
    pub cells: HashMap<(usize, usize), DataCell>, // (row, col) -> cell
    pub num_rows: usize,
    pub num_cols: usize,
    pub column_headers: Vec<String>,
}

impl Default for SpreadsheetData {
    fn default() -> Self {
        let mut headers = Vec::new();
        for i in 0..10 {
            headers.push(format!("Col {}", i + 1));
        }
        
        Self {
            cells: HashMap::new(),
            num_rows: 50,
            num_cols: 10,
            column_headers: headers,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum FitModel {
    Linear,
    Sigmoid,
    Hill,
}

impl FitModel {
    pub fn to_string(&self) -> &'static str {
        match self {
            FitModel::Linear => "Linear (y = ax + b)",
            FitModel::Sigmoid => "Sigmoid (y = a / (1 + exp(-b(x-c))))",
            FitModel::Hill => "Hill (y = (a * x^n) / (k^n + x^n))",
        }
    }
}

#[derive(Debug, Clone)]
pub struct FitResult {
    pub model: FitModel,
    pub parameters: Vec<f64>,
    pub parameter_names: Vec<String>,
    pub r_squared: f64,
    pub fitted_points: Vec<[f64; 2]>,
    pub equation_string: String,
}

#[derive(Debug, Clone)]
pub struct DataEditor {
    pub show_editor: bool,
    pub spreadsheet_data: SpreadsheetData,
    pub selected_dataset_index: usize,
    pub current_dataset: Option<Dataset>,
    pub scroll_offset: (f32, f32),
    
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

impl Default for DataEditor {
    fn default() -> Self {
        Self {
            show_editor: false,
            spreadsheet_data: SpreadsheetData::default(),
            selected_dataset_index: 0,
            current_dataset: None,
            scroll_offset: (0.0, 0.0),
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

impl DataEditor {
    pub fn show_data_editor_window(&mut self, ctx: &egui::Context, datasets: &mut Vec<Dataset>) {
        if !self.show_editor {
            return;
        }

        egui::Window::new("Data Editor")
            .resizable(true)
            .default_width(800.0)
            .default_height(600.0)
            .show(ctx, |ui| {
                // Top controls
                ui.horizontal(|ui| {
                    ui.label("Dataset:");
                    if !datasets.is_empty() {
                        egui::ComboBox::from_label("")
                            .selected_text(
                                datasets.get(self.selected_dataset_index)
                                    .map_or("None", |d| &d.name)
                            )
                            .show_ui(ui, |ui| {
                                for (i, dataset) in datasets.iter().enumerate() {
                                    ui.selectable_value(&mut self.selected_dataset_index, i, &dataset.name);
                                }
                            });
                    }
                    
                    if ui.button("Load to Editor").clicked() {
                        self.load_dataset_to_editor(datasets);
                    }
                    
                    if ui.button("Save from Editor").clicked() {
                        self.save_editor_to_dataset(datasets);
                    }
                    
                    if ui.button("Clear Editor").clicked() {
                        self.spreadsheet_data = SpreadsheetData::default();
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
                        self.spreadsheet_data.num_rows += 1;
                    }
                    
                    if ui.button("+ Add Column").clicked() {
                        self.spreadsheet_data.num_cols += 1;
                        let new_col_idx = self.spreadsheet_data.column_headers.len();
                        self.spreadsheet_data.column_headers.push(format!("Col {}", new_col_idx + 1));
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
                        }
                        
                        if ui.button("Cancel").clicked() {
                            self.show_paste_dialog = false;
                            self.paste_buffer.clear();
                        }
                    });
                }
                
                ui.separator();
                
                // Spreadsheet view
                egui::ScrollArea::both()
                    .max_height(400.0)
                    .show(ui, |ui| {
                        self.show_spreadsheet(ui);
                    });
            });
        
        // Show dialogs
        self.show_transform_dialog_window(ctx, datasets);
        self.show_fitting_dialog_window(ctx, datasets);
    }
    
    fn show_spreadsheet(&mut self, ui: &mut egui::Ui) {
        egui::Grid::new("data_spreadsheet")
            .num_columns(self.spreadsheet_data.num_cols + 1) // +1 for row headers
            .spacing([2.0, 2.0])
            .striped(true)
            .show(ui, |ui| {
                // Header row
                ui.label("Row");
                let mut header_updates = Vec::new();
                
                for (col_idx, header) in self.spreadsheet_data.column_headers.iter().enumerate() {
                    if col_idx >= self.spreadsheet_data.num_cols {
                        break;
                    }
                    let mut header_text = header.clone();
                    if ui.text_edit_singleline(&mut header_text).changed() {
                        header_updates.push((col_idx, header_text));
                    }
                }
                
                // Apply header updates after the loop
                for (col_idx, new_header) in header_updates {
                    self.spreadsheet_data.column_headers[col_idx] = new_header;
                }
                
                ui.end_row();
                
                // Data rows
                for row in 0..self.spreadsheet_data.num_rows {
                    // Row header (clickable for selection)
                    let row_header_response = ui.selectable_label(
                        self.selected_row == Some(row),
                        format!("{}", row + 1)
                    );
                    if row_header_response.clicked() {
                        self.selected_row = Some(row);
                    }
                    
                    // Data cells
                    for col in 0..self.spreadsheet_data.num_cols {
                        let cell_key = (row, col);
                        let mut cell = self.spreadsheet_data.cells
                            .get(&cell_key)
                            .cloned()
                            .unwrap_or_default();
                        
                        let old_value = cell.value.clone();
                        let response = ui.text_edit_singleline(&mut cell.value);
                        
                        if response.changed() {
                            // Try to parse as number
                            cell.parsed_value = cell.value.trim().parse::<f64>().ok();
                            self.spreadsheet_data.cells.insert(cell_key, cell.clone());
                        }
                        
                        // Visual feedback for parsing
                        if !cell.value.is_empty() && cell.parsed_value.is_none() {
                            ui.colored_label(egui::Color32::RED, "!");
                        }
                    }
                    ui.end_row();
                }
            });
    }
    
    fn parse_pasted_data(&mut self) {
        let lines: Vec<&str> = self.paste_buffer.lines().collect();
        
        for (row_offset, line) in lines.iter().enumerate() {
            let cells: Vec<&str> = if line.contains('\t') {
                line.split('\t').collect()
            } else {
                line.split(',').collect()
            };
            
            for (col_offset, cell_value) in cells.iter().enumerate() {
                let cell_key = (row_offset, col_offset);
                let mut cell = DataCell::default();
                cell.value = cell_value.trim().to_string();
                cell.parsed_value = cell.value.parse::<f64>().ok();
                
                self.spreadsheet_data.cells.insert(cell_key, cell);
                
                // Expand grid if necessary
                if col_offset >= self.spreadsheet_data.num_cols {
                    self.spreadsheet_data.num_cols = col_offset + 1;
                    while self.spreadsheet_data.column_headers.len() <= col_offset {
                        let new_col_idx = self.spreadsheet_data.column_headers.len();
                        self.spreadsheet_data.column_headers.push(format!("Col {}", new_col_idx + 1));
                    }
                }
            }
            
            if row_offset >= self.spreadsheet_data.num_rows {
                self.spreadsheet_data.num_rows = row_offset + 1;
            }
        }
        
        self.paste_buffer.clear();
    }
    
    fn load_dataset_to_editor(&mut self, datasets: &[Dataset]) {
        if let Some(dataset) = datasets.get(self.selected_dataset_index) {
            self.spreadsheet_data = SpreadsheetData::default();
            self.spreadsheet_data.num_rows = dataset.points.len().max(10);
            self.spreadsheet_data.num_cols = 2.max(self.spreadsheet_data.num_cols);
            
            // Set appropriate headers
            self.spreadsheet_data.column_headers[0] = "X".to_string();
            self.spreadsheet_data.column_headers[1] = "Y".to_string();
            
            // Load data points
            for (row, point) in dataset.points.iter().enumerate() {
                // X value
                let mut x_cell = DataCell::default();
                x_cell.value = point[0].to_string();
                x_cell.parsed_value = Some(point[0]);
                self.spreadsheet_data.cells.insert((row, 0), x_cell);
                
                // Y value
                let mut y_cell = DataCell::default();
                y_cell.value = point[1].to_string();
                y_cell.parsed_value = Some(point[1]);
                self.spreadsheet_data.cells.insert((row, 1), y_cell);
            }
        }
    }
    
    fn save_editor_to_dataset(&mut self, datasets: &mut Vec<Dataset>) {
        let mut points = Vec::new();
        
        for row in 0..self.spreadsheet_data.num_rows {
            if let (Some(x_cell), Some(y_cell)) = (
                self.spreadsheet_data.cells.get(&(row, 0)),
                self.spreadsheet_data.cells.get(&(row, 1))
            ) {
                if let (Some(x), Some(y)) = (x_cell.parsed_value, y_cell.parsed_value) {
                    points.push([x, y]);
                }
            }
        }
        
        if !points.is_empty() {
            if let Some(dataset) = datasets.get_mut(self.selected_dataset_index) {
                dataset.points = points;
            }
        }
    }
    
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
    
    fn create_dataset_from_row(&mut self, datasets: &mut Vec<Dataset>, row: usize) {
        let mut points = Vec::new();
        
        if !self.selected_x_data.is_empty() {
            // Use provided X data and row data as Y values
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
            let color = get_default_color(datasets.len() % 8);
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
                        egui::ComboBox::from_label("")
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
                    egui::ComboBox::from_label("")
                        .selected_text(self.selected_fit_model.to_string())
                        .show_ui(ui, |ui| {
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Linear, FitModel::Linear.to_string());
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Sigmoid, FitModel::Sigmoid.to_string());
                            ui.selectable_value(&mut self.selected_fit_model, FitModel::Hill, FitModel::Hill.to_string());
                        });
                });
                
                ui.separator();
                
                // Fit button
                if ui.button("🔬 Perform Fit").clicked() {
                    if let Some(dataset) = datasets.get(self.fitting_dataset_index) {
                        if let Some(fit_result) = self.perform_curve_fit(dataset) {
                            self.fit_results.push(fit_result.clone());
                            
                            // Add fitted curve as new dataset
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
                                ui.label(format!("R² = {:.4}", result.r_squared));
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
    
    fn fit_linear(&self, dataset: &Dataset) -> Option<FitResult> {
        let n = dataset.points.len() as f64;
        let sum_x: f64 = dataset.points.iter().map(|p| p[0]).sum();
        let sum_y: f64 = dataset.points.iter().map(|p| p[1]).sum();
        let sum_xy: f64 = dataset.points.iter().map(|p| p[0] * p[1]).sum();
        let sum_x2: f64 = dataset.points.iter().map(|p| p[0] * p[0]).sum();
        
        let slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
        let intercept = (sum_y - slope * sum_x) / n;
        
        // Calculate R²
        let y_mean = sum_y / n;
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
        let ss_res: f64 = dataset.points.iter().map(|p| {
            let y_pred = slope * p[0] + intercept;
            (p[1] - y_pred).powi(2)
        }).sum();
        
        let r_squared = 1.0 - (ss_res / ss_tot);
        
        // Generate fitted points
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min);
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);
        
        let mut fitted_points = Vec::new();
        for i in 0..100 {
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
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
    
    fn fit_sigmoid(&self, dataset: &Dataset) -> Option<FitResult> {
        // Simplified sigmoid fitting using linearization
        let y_min = dataset.points.iter().map(|p| p[1]).fold(f64::INFINITY, f64::min);
        let y_max = dataset.points.iter().map(|p| p[1]).fold(f64::NEG_INFINITY, f64::max);
        
        let a = y_max - y_min;
        let y_offset = y_min;
        
        // Find approximate inflection point
        let x_mid = dataset.points.iter().map(|p| p[0]).sum::<f64>() / dataset.points.len() as f64;
        
        // Rough parameter estimates
        let b = 1.0; // steepness
        let c = x_mid; // inflection point
        
        // Generate fitted points
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min);
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);
        
        let mut fitted_points = Vec::new();
        for i in 0..100 {
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
            let y = y_offset + a / (1.0 + (-b * (x - c)).exp());
            fitted_points.push([x, y]);
        }
        
        // Calculate R²
        let y_mean = dataset.points.iter().map(|p| p[1]).sum::<f64>() / dataset.points.len() as f64;
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
        let ss_res: f64 = dataset.points.iter().map(|p| {
            let y_pred = y_offset + a / (1.0 + (-b * (p[0] - c)).exp());
            (p[1] - y_pred).powi(2)
        }).sum();
        
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
    
    fn fit_hill(&self, dataset: &Dataset) -> Option<FitResult> {
        // Simplified Hill equation fitting
        // y = (a * x^n) / (k^n + x^n)
        
        let y_max = dataset.points.iter().map(|p| p[1]).fold(f64::NEG_INFINITY, f64::max);
        let a = y_max; // maximum response
        
        // Find approximate K (half-maximal concentration)
        let half_max = a / 2.0;
        let k = dataset.points.iter()
            .min_by(|p1, p2| (p1[1] - half_max).abs().partial_cmp(&(p2[1] - half_max).abs()).unwrap())
            .map(|p| p[0])
            .unwrap_or(1.0);
        
        let n = 2.0; // Hill coefficient (cooperativity)
        
        // Generate fitted points
        let x_min = dataset.points.iter().map(|p| p[0]).fold(f64::INFINITY, f64::min).max(0.001);
        let x_max = dataset.points.iter().map(|p| p[0]).fold(f64::NEG_INFINITY, f64::max);
        
        let mut fitted_points = Vec::new();
        for i in 0..100 {
            let x = x_min + (x_max - x_min) * (i as f64 / 99.0);
            if x > 0.0 {
                let y = (a * x.powf(n)) / (k.powf(n) + x.powf(n));
                fitted_points.push([x, y]);
            }
        }
        
        // Calculate R²
        let y_mean = dataset.points.iter().map(|p| p[1]).sum::<f64>() / dataset.points.len() as f64;
        let ss_tot: f64 = dataset.points.iter().map(|p| (p[1] - y_mean).powi(2)).sum();
        let ss_res: f64 = dataset.points.iter().map(|p| {
            let y_pred = if p[0] > 0.0 {
                (a * p[0].powf(n)) / (k.powf(n) + p[0].powf(n))
            } else {
                0.0
            };
            (p[1] - y_pred).powi(2)
        }).sum();
        
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
