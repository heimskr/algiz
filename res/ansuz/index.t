<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<style>
			{{css}}
		</style>
	</head>
	<body>
		<header>ᚨᚾᛊᚢᛉ</header>
		<ul>
			{% for plugin in plugins %}
				<li>{{plugin}}</li>
			{% endfor %}
		</ul>
	</body>
</html>
