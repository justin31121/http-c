function _text(response) { return response.text(); }
function _json(response) { return response.json(); }

(async () => {
    
    const blastoise = await fetch("/blastoise.json").then(_json);
    console.log(blastoise);

    /////////////////////////////////////////////////
    
    const p = document.getElementById("p");
    
    const canvas = document.getElementById("canvas");
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;

    const gl = canvas.getContext("webgl")
	  || canvas.getContext("experimental-webgl");

    if(!gl) {
	p.innerHTML = "Failed to initialize WebGL context."
	    + "Your brwoser or device may not support WebGL";
	return;
    }
    
    ///////////////////////////////////////////////////


    const VERTICIES_CAP = 128;
    const STRIDE =
	  2 + // position
	  4 + // color
	  2   // uv;
    const buffer = new ArrayBuffer(VERTICIES_CAP *
				   Float32Array.BYTES_PER_ELEMENT *
				   STRIDE );
    const verticies = new Float32Array(buffer);
    let verticies_count = 0;

    push_vertex = (x, y, r, g, b, a, uv1, uv2) => {
	if(verticies_count >= VERTICIES_CAP) return;
	let i = STRIDE * verticies_count++;
	verticies[i++] = x;
	verticies[i++] = y;
	verticies[i++] = r;
	verticies[i++] = g;
	verticies[i++] = b;
	verticies[i++] = a;
	verticies[i++] = uv1;
	verticies[i++] = uv2;
    };

    const vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, verticies, gl.DYNAMIC_DRAW, 0,
		  VERTICIES_CAP);
    
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false,
			   STRIDE * Float32Array.BYTES_PER_ELEMENT, 0);
    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 4, gl.FLOAT, false,
			   STRIDE * Float32Array.BYTES_PER_ELEMENT,
			   2 * Float32Array.BYTES_PER_ELEMENT);
    gl.enableVertexAttribArray(2);
    gl.vertexAttribPointer(2, 2, gl.FLOAT, false,
			   STRIDE * Float32Array.BYTES_PER_ELEMENT,
			   6 * Float32Array.BYTES_PER_ELEMENT);

    const [vertex_shader_source, fragment_shader_source] =
	  await Promise.all( ["/vertex.glsl", "fragment.glsl"].map(x => fetch(x).then(_text)) );

    const vertex_shader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertex_shader, vertex_shader_source);
    gl.compileShader(vertex_shader);
    if(!gl.getShaderParameter(vertex_shader, gl.COMPILE_STATUS)) {
	const compile_error_log = gl.getShaderInfoLog(vertex_shader);
	p.innerHTML = "Failed to compile vertex-shader.<br />"
	    + compile_error_log;
	return;
    }
    
    const fragment_shader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragment_shader, fragment_shader_source);
    gl.compileShader(fragment_shader);
    if(!gl.getShaderParameter(fragment_shader, gl.COMPILE_STATUS)) {
	const compile_error_log = gl.getShaderInfoLog(fragment_shader);
	p.innerHTML = "Failed to compile fragment-shader.<br />"
	    + compile_error_log;
	return;
    }

    const program = gl.createProgram();
    gl.attachShader(program, vertex_shader);
    gl.attachShader(program, fragment_shader);

    gl.bindAttribLocation(program, 0, "position");
    gl.bindAttribLocation(program, 1, "color");
    gl.bindAttribLocation(program, 2, "uv");
    
    gl.linkProgram(program);
    if(!gl.getProgramParameter(program, gl.LINK_STATUS)) {
	const link_error_log = gl.getProgramInfoLog(program);
	p.innerHTML = "Failed to link shader-program.<br />"
	    + link_error_log;
	return;
    }
    gl.useProgram(program);

    gl.genTextures(1, &r->textures);
    gl.BindTexture(GL_TEXTURE_2D, r->textures);

    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);

    gl.TexImage2D(GL_TEXTURE_2D,
		 0,
		 GL_RGBA,
		 width,
		 height,
		 0,
		 GL_RGBA,
		 GL_UNSIGNED_INT_8_8_8_8_REV,
		 data);

    let updateId = 0;
    let previousDelta = 0;
    let fpsLimit = 60;

    let acc = 0;
    let i = 0;

    function update(currentDelta) {
	updateId = requestAnimationFrame(update);

	const delta = currentDelta - previousDelta;

	if(fpsLimit && delta < 1000 / fpsLimit) {
	    return;
	}

	p.innerHTML = delta + " ms";	

	//here
	gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
	gl.clearColor(0, 0, 0, 1);
	gl.clear(gl.COLOR_BUFFER_BIT);

	/*
	{
	    const x = Math.sin(acc * 2 * Math.PI / 1000);
	    const y = Math.cos(acc * 4 * Math.PI / 1000);

	    push_vertex(-1, -1,
			1, 0, 0, 1,
			-1, -1);

	    push_vertex(x, y,
			0, 1, 0, 1,
			-1, -1);

	    push_vertex(0, 0,
			0, 0, 1, 1,
			-1, -1);
	}
	*/
	
	gl.bufferSubData(gl.ARRAY_BUFFER, 0, verticies, 0, verticies_count);
	gl.drawArrays(gl.TRIANGLES, 0, verticies_count);
	verticies_count = 0;

	previousDelta = currentDelta;
	acc += delta;
    }
    requestAnimationFrame(update);
    
})();
