<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<meta charset="utf-8" />
		<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" integrity="sha384-1BmE4kWBq78iYhFldvKuhfTAU6auU8tT94WrHftjDbrCEXSU1oBoqyl2QvZ6jIW3" crossorigin="anonymous">
		<script src="https://cdnjs.cloudflare.com/ajax/libs/jsoneditor/9.7.4/jsoneditor.min.js" integrity="sha512-KQaWlVsZF0iPXCR8p176hVrg/rlw+smy8dpJ+vwiuvoHyGr8PTVvaAV4ZmsAYdCvMRP8e3sB9pl1mRuaunaXrg==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
		<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/jsoneditor/9.7.4/jsoneditor.min.css" />
		<style>
			{{css}}
		</style>
	</head>
	<body>
		<header><a href="/ansuz">ᚨᚾᛊᚢᛉ</a></header>
		<div id="links">
			<a href="/" id="home">ᚺᛖᛁᛗ</a>
			<a href="/ansuz/load" id="load">ᛚᛟᚨᛞ</a>
		</div>
		<main class="container">
			<button class="btn btn-primary" onclick="postEdit()">Submit</button><br /><br />
			<div id="editor"></div>
		</main>
		<script>
			const container = document.getElementById("editor");
			const editor = new JSONEditor(container, {});
			editor.set({{config}});

			function postEdit() {
				const form = document.createElement("form");
				form.action = "/ansuz/edit";
				form.method = "post";
				form.style.display = "none";
				const plugin_name = document.createElement("input");
				plugin_name.type = "text";
				plugin_name.name = "pluginName";
				plugin_name.value = "{{pluginName}}";
				const plugin_data = document.createElement("input");
				plugin_data.type = "text";
				plugin_data.name = "pluginConfig";
				plugin_data.value = JSON.stringify(editor.get());
				form.appendChild(plugin_name);
				form.appendChild(plugin_data);
				document.body.appendChild(form);
				form.submit();
			}
		</script>
	</body>
</html>
