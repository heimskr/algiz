<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<meta charset="utf-8" />
		<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" integrity="sha384-1BmE4kWBq78iYhFldvKuhfTAU6auU8tT94WrHftjDbrCEXSU1oBoqyl2QvZ6jIW3" crossorigin="anonymous">
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
			<table class="table text-white">
				<thead>
					<th scope="col">Plugin</th>
					<th scope="col" colspan="2">Version</th>
				</thead>
				<tbody>
				{% for plugin in plugins %}
					<tr>
						<td class="col-10" title="{{plugin.2}}">
							<a class="edit-link" href="/ansuz/edit/{{plugin.0}}">{{plugin.1}}</a>
						</td>
						<td class="col-2">{{plugin.3}}</td>
						<td class="col-2"><a class="btn btn-primary" role="button" href="/ansuz/unload/{{plugin.0}}">Unload</a></td>
					</tr>
				{% endfor %}
				</tbody>
			</table>
		</main>
	</body>
</html>
