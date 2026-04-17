extends StaticBody3D # <--- IMPORTANTE: Cámbialo si tu script hereda de otro tipo, según tu árbol es un StaticBody3D

# > Desarrollado por PimpoliDev / Developed by PimpoliDev

@export_group("Configuración de Textura")
@export var tamaño_textura: int = 1024
@export var celdas_principales: int = 4
@export var celdas_internas: int = 4
@export var grosor_linea: int = 0

@export_group("Colores")
@export var color_oscuro_base: Color = Color(0.23, 0.27, 0.34)
@export var color_claro_base: Color = Color(0.204, 0.234, 0.271, 1.0)
@export var color_linea_rejilla: Color = Color(0.0, 0.0, 0.0, 1.0)

@export_group("Configuración Suelo")
@export var repeticion_uv: Vector3 = Vector3(100, 100, 1) # Cuántas veces se repite en el BasePlate

func _ready():
	# 1. Generar la textura procedural
	var textura: ImageTexture = crear_textura_mosaico()
	
	# 2. Buscar el MeshInstance3D hijo de BasePlate ROBUTSAMENTE
	var base_plate_mesh: MeshInstance3D = null
	
	# Iteramos sobre los hijos para encontrar el Mesh por tipo, no por nombre.
	# Esto soluciona el problema de nombres automaticos como "@MeshInstance3D@19"
	for child in get_children():
		if child is MeshInstance3D:
			base_plate_mesh = child
			break # Encontramos el primero y salimos
	
	# 3. Aplicar material si encontramos el Mesh
	if base_plate_mesh:
		aplicar_al_suelo(base_plate_mesh, textura)
		print("[Motor] Textura aplicada correctamente al BasePlate")
	else:
		# Esto no debería pasar ahora, pero por si acaso.
		push_error("[Error] No se encontró un hijo de tipo MeshInstance3D bajo el BasePlate")

func crear_textura_mosaico() -> ImageTexture:
	var imagen_generada: Image = Image.create(tamaño_textura, tamaño_textura, false, Image.FORMAT_RGBA8)
	
	var tam_celda_p: float = float(tamaño_textura) / float(celdas_principales)
	var tam_celda_i: float = tam_celda_p / float(celdas_internas)
	
	for y in range(tamaño_textura):
		for x in range(tamaño_textura):
			var color_pixel: Color
			
			var c_x_p: int = int(x / tam_celda_p)
			var c_y_p: int = int(y / tam_celda_p)
			
			if (c_x_p + c_y_p) % 2 == 0:
				color_pixel = color_oscuro_base
			else:
				color_pixel = color_claro_base
			
			var es_linea_x: bool = int(x) % int(tam_celda_i) < grosor_linea
			var es_linea_y: bool = int(y) % int(tam_celda_i) < grosor_linea
			
			if es_linea_x or es_linea_y:
				color_pixel = color_linea_rejilla
			
			imagen_generada.set_pixel(x, y, color_pixel)
			
	return ImageTexture.create_from_image(imagen_generada)

func aplicar_al_suelo(mesh_node: MeshInstance3D, tex: Texture2D):
	# Creamos un material estándar de Godot 3D
	var material: StandardMaterial3D = StandardMaterial3D.new()
	material.albedo_texture = tex
	material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST # Look pixel-perfect nítido
	material.uv1_scale = repeticion_uv # Hace que la textura se repita
	material.uv1_triplanar = false 
	# Aplicar el material al primer slot del Mesh
	mesh_node.set_surface_override_material(0, material)
