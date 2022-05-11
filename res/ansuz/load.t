<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<meta charset="utf-8" />
		<link rel="stylesheet" href="/ansuz/bootstrap.min.css" />
		<link rel="stylesheet" href="/ansuz/bootstrap-utilities.min.css" />
		<script src="https://cdnjs.cloudflare.com/ajax/libs/jsoneditor/9.7.4/jsoneditor.min.js" integrity="sha512-KQaWlVsZF0iPXCR8p176hVrg/rlw+smy8dpJ+vwiuvoHyGr8PTVvaAV4ZmsAYdCvMRP8e3sB9pl1mRuaunaXrg==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
		<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/jsoneditor/9.7.4/jsoneditor.min.css" />
		<style>
			{{css}}
		</style>
	</head>
	<body>
		<header><a href="/ansuz">ᚨᚾᛊᚢᛉ</a></header>
		<a href="/" id="home">ᚺᛖᛁᛗ</a>
		<main class="container">
			{% if length(plugins) == 0 %}
			No plugins available.
			<form onsubmit="loadPlugin(this.customPath.value); return false">
				<input type="text" name="customPath" placeholder="Plugin path" />
			</form><br />
			{% else %}
			<table class="table text-white">
				<thead>
					<th scope="col-12">Plugin</th>
					<th scope="col-2">
						<form onsubmit="loadPlugin(this.customPath.value); return false">
							<input type="text" name="customPath" placeholder="Plugin path" />
						</form>
					</th>
				</thead>
				<tbody>
				{% for plugin in plugins %}
					<tr>
						<td class="col-12">{{plugin.0}}</td>
						<td class="col-2">
							<button class="btn btn-primary" role="button" onclick="loadPlugin(&quot;{{plugin.1}}&quot;)">Load</button>
						</td>
					</tr>
				{% endfor %}
				</tbody>
			</table>
			{% endif %}
			<div id="editor" style="min-height: 300px;"></div>
		</main>
		<script>
			const container = document.getElementById("editor");
			const editor = new JSONEditor(container, {});
			editor.set({});
			function loadPlugin(name) {
				name = unescape(name);
				// Pain.
				const form = document.createElement("form");
				form.action = "/ansuz/load";
				form.name = "pluginForm";
				form.method = "post";
				form.style.display = "none";
				const plugin_name = document.createElement("input");
				plugin_name.type = "text";
				plugin_name.name = "pluginName";
				plugin_name.value = name;
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
